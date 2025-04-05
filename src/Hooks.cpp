#include "Hooks.h"
#include "Config.h"
#include "MsgSendHook.h"
#include "PatternScanner.h"
#include <iostream>
#include <optional> // For std::optional
#include "AppState.h"   // Include header for global state variables (UI, Hooks, etc.)
#include "ImGuiManager.h"
#include "MsgRecvHook.h"
#include "../ImGui/imgui.h" // Need direct access to ImGui::GetIO()
#include "../ImGui/imgui_impl_win32.h"
#include "../ImGui/imgui_impl_dx11.h"

// Forward declaration of WndProc
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Globals specific to Hooks.cpp
typedef long(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
Present originalPresent = nullptr;
Present targetPresent = nullptr;

// Function pointers and state
static bool isInit = false;
static HWND window = NULL;
static ID3D11Device* pDevice = NULL;
static ID3D11DeviceContext* pContext = NULL;
static ID3D11RenderTargetView* mainRenderTargetView = NULL;
static WNDPROC originalWndProc = nullptr;

// Hooked Present function
static HRESULT __stdcall DetourPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    // Immediately return if shutdown is in progress to prevent race conditions
    if (kx::g_isShuttingDown) {
        return originalPresent(pSwapChain, SyncInterval, Flags);
    }
    if (!isInit) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&pDevice)))) {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;

            ID3D11Texture2D* pBackBuffer = nullptr;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
            if (pBackBuffer) {
                pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
                pBackBuffer->Release();
            }

            // Replace the window's WndProc with our custom one
            originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc));

            // Initialize ImGui
            if (!ImGuiManager::Initialize(pDevice, pContext, window)) {
                std::cerr << "Failed to initialize ImGui." << std::endl;
                return originalPresent(pSwapChain, SyncInterval, Flags);
            }

            isInit = true;
        }
        else {
            return originalPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    // Check for GUI toggle hotkey (INSERT)
    static bool lastToggleKeyState = false;
    bool currentToggleKeyState = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (currentToggleKeyState && !lastToggleKeyState) {
        kx::g_showInspectorWindow = !kx::g_showInspectorWindow;
    }
    lastToggleKeyState = currentToggleKeyState;

    ImGuiManager::NewFrame();

    ImGuiManager::RenderUI();

    ImGuiManager::Render(pContext, mainRenderTargetView);

    return originalPresent(pSwapChain, SyncInterval, Flags);
}

// WndProc replacement
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Give ImGui the first chance to process the message, but only if ImGui is initialized and visible.
    // We need ImGui to process messages even if we later pass them to the game,
    // so it can update its internal state (like modifier keys).
    if (isInit && kx::g_showInspectorWindow) { // Ensure ImGui is initialized (isInit flag)
        ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        // Now check if ImGui wants to capture input *after* it has processed the message.
        ImGuiIO& io = ImGui::GetIO();
        switch (uMsg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_CHAR:
            // Add other relevant keyboard messages if needed (e.g., WM_UNICHAR)
            if (io.WantCaptureKeyboard) {
                return 1; // Block message from game if ImGui wants keyboard input
            }
            break; // Message not captured by ImGui keyboard, proceed to game

        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_MOUSEMOVE:
            // Add other relevant mouse messages if needed
            if (io.WantCaptureMouse) {
                return 1; // Block message from game if ImGui wants mouse input
            }
            break; // Message not captured by ImGui mouse, proceed to game
            // Consider adding WM_SETCURSOR handling if mouse cursor issues arise
        }
        // If ImGui didn't want to capture this specific message type, fall through
        // to call the original WndProc below.
    }

    // If ImGui isn't initialized, isn't visible, or didn't want to capture the input,
    // pass the message to the original window procedure.
    // Ensure originalWndProc is valid before calling.
    if (originalWndProc) {
        return CallWindowProc(originalWndProc, hWnd, uMsg, wParam, lParam);
    }
    else {
        // Fallback if originalWndProc is somehow null (shouldn't happen in normal flow)
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

// Function to get the Present pointer
bool get_present_pointer(Present& outPresent) {
    const wchar_t* DUMMY_WNDCLASS_NAME = L"KxDummyWindow";
    HWND dummy_hwnd = NULL;
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProcW, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, DUMMY_WNDCLASS_NAME, NULL };

    // Register dummy window class
    if (!RegisterClassExW(&wc)) {
        std::cerr << "[get_present_pointer] Failed to register dummy window class. Error code: " << GetLastError() << std::endl;
        return false;
    }

    // Create dummy window
    dummy_hwnd = CreateWindowW(DUMMY_WNDCLASS_NAME, NULL, WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    if (!dummy_hwnd) {
        std::cerr << "[get_present_pointer] Failed to create dummy window. Error code: " << GetLastError() << std::endl;
        UnregisterClassW(DUMMY_WNDCLASS_NAME, wc.hInstance);
        return false;
    }
    std::cout << "[get_present_pointer] Created dummy HWND: " << dummy_hwnd << std::endl;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = dummy_hwnd; // Use the dummy window handle
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* swapChain = nullptr;
    ID3D11Device* device = nullptr;
    bool success = false;

    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0, // Use 0 for flags unless debugging D3D layer
        featureLevels,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &swapChain,
        &device,
        nullptr,
        nullptr);

    if (hr == S_OK) {
        void** vTable = *reinterpret_cast<void***>(swapChain);
        outPresent = reinterpret_cast<Present>(vTable[8]);
        swapChain->Release();
        device->Release();
        success = true;
        std::cout << "[get_present_pointer] Successfully retrieved Present pointer." << std::endl;
    } else {
        std::cerr << "[get_present_pointer] D3D11CreateDeviceAndSwapChain failed with HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        // Clean up potentially partially created resources if they exist
        if (swapChain) swapChain->Release();
        if (device) device->Release();
        success = false;
    }

    // Cleanup dummy window and class regardless of success
    if (dummy_hwnd) {
        DestroyWindow(dummy_hwnd);
    }
    UnregisterClassW(DUMMY_WNDCLASS_NAME, wc.hInstance);
    std::cout << "[get_present_pointer] Cleaned up dummy window and class." << std::endl;

    return success;
}

// Initialize hooks
bool InitializeHooks() {
    // Reset status at the start
    kx::g_presentHookStatus = kx::HookStatus::Failed;
    kx::g_msgSendHookStatus = kx::HookStatus::Unknown; // Use Unknown initially
    kx::g_msgSendAddress = 0;
    kx::g_msgRecvHookStatus = kx::HookStatus::Unknown; // Initialize placeholder
    kx::g_msgRecvAddress = 0;                          // Initialize placeholder

    if (MH_Initialize() != MH_OK) {
        std::cerr << "Failed to initialize MinHook." << std::endl;
        return false; // Keep present status as Failed
    }

    // Hook Present
    if (!get_present_pointer(targetPresent)) {
        std::cerr << "Failed to get Present pointer." << std::endl;
        MH_Uninitialize(); // Clean up MinHook if we fail here
        return false; // Keep present status as Failed
    }

    if (MH_CreateHook(targetPresent, DetourPresent, reinterpret_cast<void**>(&originalPresent)) != MH_OK) {
        std::cerr << "Failed to create Present hook." << std::endl;
        MH_Uninitialize();
        return false; // Keep present status as Failed
    }

    if (MH_EnableHook(targetPresent) != MH_OK) {
        std::cerr << "Failed to enable Present hook." << std::endl;
        MH_RemoveHook(targetPresent); // Clean up created hook
        MH_Uninitialize();
        return false; // Keep present status as Failed
    }
    kx::g_presentHookStatus = kx::HookStatus::OK; // Present hook succeeded

    // Find and Hook MsgSend using Pattern Scanner
    std::cout << "Scanning for MsgSend pattern..." << std::endl;
    std::optional<uintptr_t> msgSendAddrOpt = kx::PatternScanner::FindPattern(
        std::string(kx::MSG_SEND_PATTERN),
        std::string(kx::TARGET_PROCESS_NAME)
    );

    kx::g_msgSendHookStatus = kx::HookStatus::Failed; // Assume failure unless explicitly successful
    if (!msgSendAddrOpt) {
        std::cerr << "MsgSend pattern not found in " << kx::TARGET_PROCESS_NAME << ". Cannot hook MsgSend." << std::endl;
        // Status remains Failed, address remains 0
        // Decide if this is fatal. For now, let's allow proceeding without the MsgSend hook.
        // If it *is* fatal, uncomment the next lines:
        // MH_DisableHook(targetPresent); // Clean up the Present hook if we fail here
        // MH_RemoveHook(targetPresent);
        // return false;
    } else {
        kx::g_msgSendAddress = *msgSendAddrOpt; // Store the found address
        std::cout << "MsgSend pattern found at address: 0x" << std::hex << kx::g_msgSendAddress << std::dec << std::endl;
        if (!InitializeMsgSendHook(kx::g_msgSendAddress)) {
            std::cerr << "Failed to initialize MsgSend hook." << std::endl;
            // Status remains Failed
            // Decide if this is fatal. Current logic allows continuing.
            // If fatal:
            // MH_DisableHook(targetPresent);
            // MH_RemoveHook(targetPresent);
            // kx::g_presentHookStatus = kx::HookStatus::Failed; // Mark Present as failed too if this is critical
            // MH_Uninitialize();
            // return false;
        }
        kx::g_msgSendHookStatus = kx::HookStatus::OK; // MsgSend hook succeeded
        std::cout << "MsgSend hook initialized." << std::endl;
    }

    // --- Find and Hook MsgRecv ---
    std::cout << "Scanning for MsgRecv pattern..." << std::endl;
    std::optional<uintptr_t> msgRecvAddrOpt = kx::PatternScanner::FindPattern(
        std::string(kx::MSG_RECV_PATTERN),
        std::string(kx::TARGET_PROCESS_NAME)
    );

    kx::g_msgRecvHookStatus = kx::HookStatus::Failed; // Assume failure
    if (!msgRecvAddrOpt) {
        std::cerr << "MsgRecv pattern not found. MsgRecv hooking skipped." << std::endl;
        // Decide if fatal (current logic: not fatal)
    }
    else {
        kx::g_msgRecvAddress = *msgRecvAddrOpt;
        std::cout << "MsgRecv pattern found at: 0x" << std::hex << kx::g_msgRecvAddress << std::dec << std::endl;
        if (InitializeMsgRecvHook(kx::g_msgRecvAddress)) {
            kx::g_msgRecvHookStatus = kx::HookStatus::OK;
            std::cout << "MsgRecv hook initialized." << std::endl;
        }
        else {
            std::cerr << "Failed to initialize MsgRecv hook. Hooking skipped." << std::endl;
            // Status remains Failed
            // Decide if fatal (current logic: not fatal)
        }
    }

    return true;
}

// Cleanup hooks
void CleanupHooks() {
    // Disable all hooks
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    // Cleanup ImGui
    ImGuiManager::Shutdown();

    if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = NULL; }
    if (pContext) { pContext->Release(); pContext = NULL; }
    if (pDevice) { pDevice->Release(); pDevice = NULL; }

    if (window && originalWndProc) {
        SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalWndProc));
    }

    // Cleanup MsgSend hook
    CleanupMsgSendHook();
	CleanupMsgRecvHook();
}
