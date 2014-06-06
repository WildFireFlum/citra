// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <map>
#include <string>

#include "common/symbols.h"

#include "core/mem_map.h"

#include "core/hle/kernel/event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/thread.h"

#include "core/hle/function_wrappers.h"
#include "core/hle/svc.h"
#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SVC

namespace SVC {

enum ControlMemoryOperation {
    MEMORY_OPERATION_HEAP       = 0x00000003,
    MEMORY_OPERATION_GSP_HEAP   = 0x00010003,
};

enum MapMemoryPermission {
    MEMORY_PERMISSION_UNMAP     = 0x00000000,
    MEMORY_PERMISSION_NORMAL    = 0x00000001,
};

/// Map application or GSP heap memory
Result ControlMemory(void* _out_addr, u32 operation, u32 addr0, u32 addr1, u32 size, u32 permissions) {
    u32* out_addr = (u32*)_out_addr;

    DEBUG_LOG(SVC,"called operation=0x%08X, addr0=0x%08X, addr1=0x%08X, size=%08X, permissions=0x%08X", 
        operation, addr0, addr1, size, permissions);

    switch (operation) {

    // Map normal heap memory
    case MEMORY_OPERATION_HEAP:
        *out_addr = Memory::MapBlock_Heap(size, operation, permissions);
        break;

    // Map GSP heap memory
    case MEMORY_OPERATION_GSP_HEAP:
        *out_addr = Memory::MapBlock_HeapGSP(size, operation, permissions);
        break;

    // Unknown ControlMemory operation
    default:
        ERROR_LOG(SVC, "unknown operation=0x%08X", operation);
    }
    return 0;
}

/// Maps a memory block to specified address
Result MapMemoryBlock(Handle memblock, u32 addr, u32 mypermissions, u32 otherpermission) {
    DEBUG_LOG(SVC, "called memblock=0x08X, addr=0x%08X, mypermissions=0x%08X, otherpermission=%d", 
        memblock, addr, mypermissions, otherpermission);
    switch (mypermissions) {
    case MEMORY_PERMISSION_NORMAL:
    case MEMORY_PERMISSION_NORMAL + 1:
    case MEMORY_PERMISSION_NORMAL + 2:
        Memory::MapBlock_Shared(memblock, addr, mypermissions);
        break;
    default:
        ERROR_LOG(OSHLE, "unknown permissions=0x%08X", mypermissions);
    }
    return 0;
}

/// Connect to an OS service given the port name, returns the handle to the port to out
Result ConnectToPort(void* _out, const char* port_name) {
    Handle* out = (Handle*)_out;
    Service::Interface* service = Service::g_manager->FetchFromPortName(port_name);

    DEBUG_LOG(SVC, "called port_name=%s", port_name);
    _assert_msg_(KERNEL, (service != nullptr), "called, but service is not implemented!");

    *out = service->GetHandle();

    return 0;
}

/// Synchronize to an OS service
Result SendSyncRequest(Handle handle) {
    Kernel::Object* object = Kernel::g_object_pool.GetFast<Kernel::Object>(handle);

    _assert_msg_(KERNEL, (object != nullptr), "called, but kernel object is nullptr!");
    DEBUG_LOG(SVC, "called handle=0x%08X(%s)", handle, object->GetTypeName());

    bool wait = false;
    Result res = object->SyncRequest(&wait);
    if (wait) {
        Kernel::WaitCurrentThread(WAITTYPE_SYNCH); // TODO(bunnei): Is this correct?
    }

    return res;
}

/// Close a handle
Result CloseHandle(Handle handle) {
    // ImplementMe
    ERROR_LOG(SVC, "(UNIMPLEMENTED) called handle=0x%08X", handle);
    return 0;
}

/// Wait for a handle to synchronize, timeout after the specified nanoseconds
Result WaitSynchronization1(Handle handle, s64 nano_seconds) {
    // TODO(bunnei): Do something with nano_seconds, currently ignoring this
    bool wait = false;
    bool wait_infinite = (nano_seconds == -1); // Used to wait until a thread has terminated

    Kernel::Object* object = Kernel::g_object_pool.GetFast<Kernel::Object>(handle);

    DEBUG_LOG(SVC, "called handle=0x%08X(%s:%s), nanoseconds=%d", handle, object->GetTypeName(), 
            object->GetName(), nano_seconds);

    _assert_msg_(KERNEL, (object != nullptr), "called, but kernel object is nullptr!");

    Result res = object->WaitSynchronization(&wait);

    // Check for next thread to schedule
    if (wait) {
        HLE::Reschedule(__func__);
        return 0;
    }

    return res;
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
Result WaitSynchronizationN(void* _out, void* _handles, u32 handle_count, u32 wait_all, 
    s64 nano_seconds) {
    // TODO(bunnei): Do something with nano_seconds, currently ignoring this
    s32* out = (s32*)_out;
    Handle* handles = (Handle*)_handles;
    bool unlock_all = true;
    bool wait_infinite = (nano_seconds == -1); // Used to wait until a thread has terminated

    DEBUG_LOG(SVC, "called handle_count=%d, wait_all=%s, nanoseconds=%d", 
        handle_count, (wait_all ? "true" : "false"), nano_seconds);

    // Iterate through each handle, synchronize kernel object
    for (u32 i = 0; i < handle_count; i++) {
        bool wait = false;
        Kernel::Object* object = Kernel::g_object_pool.GetFast<Kernel::Object>(handles[i]);

        _assert_msg_(KERNEL, (object != nullptr), "called handle=0x%08X, but kernel object "
            "is nullptr!", handles[i]);

        DEBUG_LOG(SVC, "\thandle[%d] = 0x%08X(%s:%s)", i, handles[i], object->GetTypeName(), 
            object->GetName());

        Result res = object->WaitSynchronization(&wait);

        if (!wait && !wait_all) {
            *out = i;
            return 0;
        } else {
            unlock_all = false;
        }
    }

    if (wait_all && unlock_all) {
        *out = handle_count;
        return 0;
    }

    // Check for next thread to schedule
    HLE::Reschedule(__func__);

    return 0;
}

/// Create an address arbiter (to allocate access to shared resources)
Result CreateAddressArbiter(void* arbiter) {
    ERROR_LOG(SVC, "(UNIMPLEMENTED) called");
    Core::g_app_core->SetReg(1, 0xFABBDADD);
    return 0;
}

/// Arbitrate address
Result ArbitrateAddress(Handle arbiter, u32 addr, u32 _type, u32 value, s64 nanoseconds) {
    ERROR_LOG(SVC, "(UNIMPLEMENTED) called");
    ArbitrationType type = (ArbitrationType)_type;
    Memory::Write32(addr, type);
    return 0;
}

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
void OutputDebugString(const char* string) {
    OS_LOG(SVC, "%s", string);
}

/// Get resource limit
Result GetResourceLimit(void* _resource_limit, Handle process) {
    // With regards to proceess values:
    // 0xFFFF8001 is a handle alias for the current KProcess, and 0xFFFF8000 is a handle alias for 
    // the current KThread.
    Handle* resource_limit = (Handle*)_resource_limit;
    *resource_limit = 0xDEADBEEF;
    ERROR_LOG(SVC, "(UNIMPLEMENTED) called process=0x%08X", process);
    return 0;
}

/// Get resource limit current values
Result GetResourceLimitCurrentValues(void* _values, Handle resource_limit, void* names, 
    s32 name_count) {
    ERROR_LOG(SVC, "(UNIMPLEMENTED) called resource_limit=%08X, names=%s, name_count=%d",
        resource_limit, names, name_count);
    Memory::Write32(Core::g_app_core->GetReg(0), 0); // Normmatt: Set used memory to 0 for now
    return 0;
}

/// Creates a new thread
Result CreateThread(u32 priority, u32 entry_point, u32 arg, u32 stack_top, u32 processor_id) {
    std::string name;
    if (Symbols::HasSymbol(entry_point)) {
        TSymbol symbol = Symbols::GetSymbol(entry_point);
        name = symbol.name;
    } else {
        char buff[100];
        sprintf(buff, "%s", "unknown-%08X", entry_point);
        name = buff;
    }

    Handle thread = Kernel::CreateThread(name.c_str(), entry_point, priority, arg, processor_id,
        stack_top);

    Core::g_app_core->SetReg(1, thread);

    DEBUG_LOG(SVC, "called entrypoint=0x%08X (%s), arg=0x%08X, stacktop=0x%08X, "
        "threadpriority=0x%08X, processorid=0x%08X : created handle=0x%08X", entry_point, 
        name.c_str(), arg, stack_top, priority, processor_id, thread);
    
    return 0;
}

/// Called when a thread exits
u32 ExitThread() {
    Handle thread = Kernel::GetCurrentThreadHandle();

    DEBUG_LOG(SVC, "called, pc=0x%08X", Core::g_app_core->GetPC()); // PC = 0x0010545C

    Kernel::StopThread(thread, __func__);
    HLE::Reschedule(__func__);
    return 0;
}

/// Gets the priority for the specified thread
Result GetThreadPriority(void* _priority, Handle handle) {
    s32* priority = (s32*)_priority;
    *priority = Kernel::GetThreadPriority(handle);
    return 0;
}

/// Sets the priority for the specified thread
Result SetThreadPriority(Handle handle, s32 priority) {
    return Kernel::SetThreadPriority(handle, priority);
}

/// Create a mutex
Result CreateMutex(void* _mutex, u32 initial_locked) {
    Handle* mutex = (Handle*)_mutex;
    *mutex = Kernel::CreateMutex((initial_locked != 0));
    DEBUG_LOG(SVC, "called initial_locked=%s : created handle=0x%08X", 
        initial_locked ? "true" : "false", *mutex);
    return 0;
}

/// Release a mutex
Result ReleaseMutex(Handle handle) {
    DEBUG_LOG(SVC, "called handle=0x%08X", handle);
    _assert_msg_(KERNEL, (handle != 0), "called, but handle is nullptr!");
    Kernel::ReleaseMutex(handle);
    return 0;
}

/// Get current thread ID
Result GetThreadId(void* thread_id, u32 thread) {
    ERROR_LOG(SVC, "(UNIMPLEMENTED) called thread=0x%08X", thread);
    return 0;
}

/// Query memory
Result QueryMemory(void *_info, void *_out, u32 addr) {
    ERROR_LOG(SVC, "(UNIMPLEMENTED) called addr=0x%08X", addr);
    return 0;
}

/// Create an event
Result CreateEvent(void* _event, u32 reset_type) {
    Handle* evt = (Handle*)_event;
    *evt = Kernel::CreateEvent((ResetType)reset_type);
    DEBUG_LOG(SVC, "called reset_type=0x%08X : created handle=0x%08X", 
        reset_type, *evt);
    return 0;
}

/// Duplicates a kernel handle
Result DuplicateHandle(void* _out, Handle handle) {
    Handle* out = (Handle*)_out;
    DEBUG_LOG(SVC, "called handle=0x%08X", handle);

    // Translate kernel handles -> real handles
    if (handle == Kernel::CurrentThread) {
        handle = Kernel::GetCurrentThreadHandle();
    }
    _assert_msg_(KERNEL, (handle != Kernel::CurrentProcess),
        "(UNIMPLEMENTED) process handle duplication!");
    
    // TODO(bunnei): FixMe - This is a hack to return the handle that we were asked to duplicate.
    *out = handle;

    return 0;
}

/// Signals an event
Result SignalEvent(Handle evt) {
    Result res = Kernel::SignalEvent(evt);
    DEBUG_LOG(SVC, "called event=0x%08X", evt);
    return res;
}

/// Clears an event
Result ClearEvent(Handle evt) {
    Result res = Kernel::ClearEvent(evt);
    DEBUG_LOG(SVC, "called event=0x%08X", evt);
    return res;
}

/// Sleep the current thread
void SleepThread(s64 nanoseconds) {
    DEBUG_LOG(SVC, "called nanoseconds=%d", nanoseconds);
}

const HLE::FunctionDef SVC_Table[] = {
    {0x00,  nullptr,                                    "Unknown"},
    {0x01,  WrapI_VUUUUU<ControlMemory>,                "ControlMemory"},
    {0x02,  WrapI_VVU<QueryMemory>,                     "QueryMemory"},
    {0x03,  nullptr,                                    "ExitProcess"},
    {0x04,  nullptr,                                    "GetProcessAffinityMask"},
    {0x05,  nullptr,                                    "SetProcessAffinityMask"},
    {0x06,  nullptr,                                    "GetProcessIdealProcessor"},
    {0x07,  nullptr,                                    "SetProcessIdealProcessor"},
    {0x08,  WrapI_UUUUU<CreateThread>,                  "CreateThread"},
    {0x09,  WrapU_V<ExitThread>,                        "ExitThread"},
    {0x0A,  WrapV_S64<SleepThread>,                     "SleepThread"},
    {0x0B,  WrapI_VU<GetThreadPriority>,                "GetThreadPriority"},
    {0x0C,  WrapI_UI<SetThreadPriority>,                "SetThreadPriority"},
    {0x0D,  nullptr,                                    "GetThreadAffinityMask"},
    {0x0E,  nullptr,                                    "SetThreadAffinityMask"},
    {0x0F,  nullptr,                                    "GetThreadIdealProcessor"},
    {0x10,  nullptr,                                    "SetThreadIdealProcessor"},
    {0x11,  nullptr,                                    "GetCurrentProcessorNumber"},
    {0x12,  nullptr,                                    "Run"},
    {0x13,  WrapI_VU<CreateMutex>,                      "CreateMutex"},
    {0x14,  WrapI_U<ReleaseMutex>,                      "ReleaseMutex"},
    {0x15,  nullptr,                                    "CreateSemaphore"},
    {0x16,  nullptr,                                    "ReleaseSemaphore"},
    {0x17,  WrapI_VU<CreateEvent>,                      "CreateEvent"},
    {0x18,  WrapI_U<SignalEvent>,                       "SignalEvent"},
    {0x19,  WrapI_U<ClearEvent>,                        "ClearEvent"},
    {0x1A,  nullptr,                                    "CreateTimer"},
    {0x1B,  nullptr,                                    "SetTimer"},
    {0x1C,  nullptr,                                    "CancelTimer"},
    {0x1D,  nullptr,                                    "ClearTimer"},
    {0x1E,  nullptr,                                    "CreateMemoryBlock"},
    {0x1F,  WrapI_UUUU<MapMemoryBlock>,                 "MapMemoryBlock"},
    {0x20,  nullptr,                                    "UnmapMemoryBlock"},
    {0x21,  WrapI_V<CreateAddressArbiter>,              "CreateAddressArbiter"},
    {0x22,  WrapI_UUUUS64<ArbitrateAddress>,            "ArbitrateAddress"},
    {0x23,  WrapI_U<CloseHandle>,                       "CloseHandle"},
    {0x24,  WrapI_US64<WaitSynchronization1>,           "WaitSynchronization1"},
    {0x25,  WrapI_VVUUS64<WaitSynchronizationN>,        "WaitSynchronizationN"},
    {0x26,  nullptr,                                    "SignalAndWait"},
    {0x27,  WrapI_VU<DuplicateHandle>,                  "DuplicateHandle"},
    {0x28,  nullptr,                                    "GetSystemTick"},
    {0x29,  nullptr,                                    "GetHandleInfo"},
    {0x2A,  nullptr,                                    "GetSystemInfo"},
    {0x2B,  nullptr,                                    "GetProcessInfo"},
    {0x2C,  nullptr,                                    "GetThreadInfo"},
    {0x2D,  WrapI_VC<ConnectToPort>,                    "ConnectToPort"},
    {0x2E,  nullptr,                                    "SendSyncRequest1"},
    {0x2F,  nullptr,                                    "SendSyncRequest2"},
    {0x30,  nullptr,                                    "SendSyncRequest3"},
    {0x31,  nullptr,                                    "SendSyncRequest4"},
    {0x32,  WrapI_U<SendSyncRequest>,                   "SendSyncRequest"},
    {0x33,  nullptr,                                    "OpenProcess"},
    {0x34,  nullptr,                                    "OpenThread"},
    {0x35,  nullptr,                                    "GetProcessId"},
    {0x36,  nullptr,                                    "GetProcessIdOfThread"},
    {0x37,  WrapI_VU<GetThreadId>,                      "GetThreadId"},
    {0x38,  WrapI_VU<GetResourceLimit>,                 "GetResourceLimit"},
    {0x39,  nullptr,                                    "GetResourceLimitLimitValues"},
    {0x3A,  WrapI_VUVI<GetResourceLimitCurrentValues>,  "GetResourceLimitCurrentValues"},
    {0x3B,  nullptr,                                    "GetThreadContext"},
    {0x3C,  nullptr,                                    "Break"},
    {0x3D,  WrapV_C<OutputDebugString>,                 "OutputDebugString"},
    {0x3E,  nullptr,                                    "ControlPerformanceCounter"},
    {0x3F,  nullptr,                                    "Unknown"},
    {0x40,  nullptr,                                    "Unknown"},
    {0x41,  nullptr,                                    "Unknown"},
    {0x42,  nullptr,                                    "Unknown"},
    {0x43,  nullptr,                                    "Unknown"},
    {0x44,  nullptr,                                    "Unknown"},
    {0x45,  nullptr,                                    "Unknown"},
    {0x46,  nullptr,                                    "Unknown"},
    {0x47,  nullptr,                                    "CreatePort"},
    {0x48,  nullptr,                                    "CreateSessionToPort"},
    {0x49,  nullptr,                                    "CreateSession"},
    {0x4A,  nullptr,                                    "AcceptSession"},
    {0x4B,  nullptr,                                    "ReplyAndReceive1"},
    {0x4C,  nullptr,                                    "ReplyAndReceive2"},
    {0x4D,  nullptr,                                    "ReplyAndReceive3"},
    {0x4E,  nullptr,                                    "ReplyAndReceive4"},
    {0x4F,  nullptr,                                    "ReplyAndReceive"},
    {0x50,  nullptr,                                    "BindInterrupt"},
    {0x51,  nullptr,                                    "UnbindInterrupt"},
    {0x52,  nullptr,                                    "InvalidateProcessDataCache"},
    {0x53,  nullptr,                                    "StoreProcessDataCache"},
    {0x54,  nullptr,                                    "FlushProcessDataCache"},
    {0x55,  nullptr,                                    "StartInterProcessDma"},
    {0x56,  nullptr,                                    "StopDma"},
    {0x57,  nullptr,                                    "GetDmaState"},
    {0x58,  nullptr,                                    "RestartDma"},
    {0x59,  nullptr,                                    "Unknown"},
    {0x5A,  nullptr,                                    "Unknown"},
    {0x5B,  nullptr,                                    "Unknown"},
    {0x5C,  nullptr,                                    "Unknown"},
    {0x5D,  nullptr,                                    "Unknown"},
    {0x5E,  nullptr,                                    "Unknown"},
    {0x5F,  nullptr,                                    "Unknown"},
    {0x60,  nullptr,                                    "DebugActiveProcess"},
    {0x61,  nullptr,                                    "BreakDebugProcess"},
    {0x62,  nullptr,                                    "TerminateDebugProcess"},
    {0x63,  nullptr,                                    "GetProcessDebugEvent"},
    {0x64,  nullptr,                                    "ContinueDebugEvent"},
    {0x65,  nullptr,                                    "GetProcessList"},
    {0x66,  nullptr,                                    "GetThreadList"},
    {0x67,  nullptr,                                    "GetDebugThreadContext"},
    {0x68,  nullptr,                                    "SetDebugThreadContext"},
    {0x69,  nullptr,                                    "QueryDebugProcessMemory"},
    {0x6A,  nullptr,                                    "ReadProcessMemory"},
    {0x6B,  nullptr,                                    "WriteProcessMemory"},
    {0x6C,  nullptr,                                    "SetHardwareBreakPoint"},
    {0x6D,  nullptr,                                    "GetDebugThreadParam"},
    {0x6E,  nullptr,                                    "Unknown"},
    {0x6F,  nullptr,                                    "Unknown"},
    {0x70,  nullptr,                                    "ControlProcessMemory"},
    {0x71,  nullptr,                                    "MapProcessMemory"},
    {0x72,  nullptr,                                    "UnmapProcessMemory"},
    {0x73,  nullptr,                                    "Unknown"},
    {0x74,  nullptr,                                    "Unknown"},
    {0x75,  nullptr,                                    "Unknown"},
    {0x76,  nullptr,                                    "TerminateProcess"},
    {0x77,  nullptr,                                    "Unknown"},
    {0x78,  nullptr,                                    "CreateResourceLimit"},
    {0x79,  nullptr,                                    "Unknown"},
    {0x7A,  nullptr,                                    "Unknown"},
    {0x7B,  nullptr,                                    "Unknown"},
    {0x7C,  nullptr,                                    "KernelSetState"},
    {0x7D,  nullptr,                                    "QueryProcessMemory"},
};

void Register() {
    HLE::RegisterModule("SVC_Table", ARRAY_SIZE(SVC_Table), SVC_Table);
}

} // namespace
