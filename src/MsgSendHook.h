#pragma once

/**
 * @file MsgSendHook.h
 * @brief Defines structures and functions for hooking the game's internal
 *        message sending function (FUN_1412e9960).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint> // For uintptr_t
#include "../MinHook/MinHook.h" // Adjust path as necessary

 // Link MinHook lib (consider moving linker directives to project settings)
#if _WIN64
#pragma comment(lib, "MinHook/libMinHook.x64.lib")
#else
#pragma comment(lib, "MinHook/libMinHook.x86.lib")
#endif

/**
 * @brief Defines the function pointer type for the original message sending function.
 * @details Signature based on reverse engineering FUN_1412e9960.
 *          Uses the __fastcall calling convention (first arg in RCX).
 * @param void* (RCX) Pointer to the MsgSendContext object containing packet buffer info.
 */
typedef void(__fastcall* MsgSendFunc)(void*);

/**
 * @brief Initializes the MinHook detour for the message sending function.
 * @param targetFunctionAddress The memory address of the original game function.
 * @return true If the hook was successfully created and enabled, false otherwise.
 */
bool InitializeMsgSendHook(uintptr_t targetFunctionAddress);

/**
 * @brief Disables and removes the MinHook detour for the message sending function.
 */
void CleanupMsgSendHook();

/**
 * @brief The detour function that replaces the original message sending function.
 * @details Intercepts the call, potentially delegates processing to PacketProcessor,
 *          and then calls the original function via `originalMsgSend`.
 *          Matches the signature defined by MsgSendFunc.
 * @param param_1 (RCX) Pointer to the MsgSendContext object.
 */
void __fastcall hookMsgSend(void* param_1);

/**
 * @brief Pointer to hold the address of the original game function.
 * @details Populated by MinHook; used by `hookMsgSend` to call the original code.
 */
extern MsgSendFunc originalMsgSend;