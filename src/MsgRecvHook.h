#pragma once

/**
 * @file MsgRecvHook.h
 * @brief Defines structures and functions for hooking the game's internal
 *        message receiving function (FUN_1412ea0c0).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include "../MinHook/MinHook.h" // Adjust path as necessary

 /**
  * @brief Defines the function pointer type for the original message receiving function.
  * @details Signature based on reverse engineering FUN_1412ea0c0 and caller FUN_140e8e820.
  *          Uses the __fastcall calling convention (RCX, RDX, R8, R9).
  * @param void* (RCX) Unknown purpose, potentially output/status pointer. Passed through.
  * @param void* (RDX) Pointer to the MsgConn context object. Contains connection state. Can be NULL.
  * @param unsigned char* (R8) Pointer to the start of the raw received data buffer.
  * @param unsigned long long (R9) Size of the data buffer, possibly includes flags (requires masking).
  * @param int (Stack) Unknown stack parameter. Passed through.
  * @param void* (Stack) Unknown stack parameter. Passed through.
  * @return void* Original function returns a pointer (likely status), MUST be returned by hook.
  */
typedef void* (__fastcall* MsgRecvFunc)(
    void* param_1,              // RCX
    void* param_2,              // RDX: MsgConn Context Ptr
    unsigned char* param_3,     // R8:  Packet Buffer Start Ptr
    unsigned long long param_4, // R9:  Packet Buffer Size (Masked)
    int param_5,                // Stack Arg 1
    void* param_6               // Stack Arg 2
    );

/**
 * @brief Initializes the MinHook detour for the message receiving function.
 * @param targetFunctionAddress The memory address of the original game function.
 * @return true If the hook was successfully created and enabled, false otherwise.
 */
bool InitializeMsgRecvHook(uintptr_t targetFunctionAddress);

/**
 * @brief Disables and removes the MinHook detour for the message receiving function.
 */
void CleanupMsgRecvHook();

/**
 * @brief The detour function that replaces the original message receiving function.
 * @details Intercepts the call, captures state, potentially delegates processing
 *          to PacketProcessor, and then calls the original function via `originalMsgRecv`.
 *          Matches the signature defined by MsgRecvFunc.
 * @param ... (Parameters match MsgRecvFunc)
 * @return void* (Return value matches MsgRecvFunc)
 */
void* __fastcall hookMsgRecv(
    void* param_1,
    void* param_2,
    unsigned char* param_3,
    unsigned long long param_4,
    int param_5,
    void* param_6
);

/**
 * @brief Pointer to hold the address of the original game function.
 * @details Populated by MinHook; used by `hookMsgRecv` to call the original code.
 */
extern MsgRecvFunc originalMsgRecv;