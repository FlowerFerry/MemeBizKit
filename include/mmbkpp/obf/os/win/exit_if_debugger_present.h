
#ifndef MMBKPP_OBF_OS_WIN_EXIT_IF_DEBUGGER_PRESENT_H_INCLUDED
#define MMBKPP_OBF_OS_WIN_EXIT_IF_DEBUGGER_PRESENT_H_INCLUDED

#include <mego/util/os/windows/windows_simplify.h>

#if MG_OS__WIN_AVAIL
#include <bcrypt.h>
#endif

#include <mmbkpp/wrap/obfy/instr.h>

namespace mmbkpp {
namespace obf_os {
namespace win {

#if MG_OS__WIN_AVAIL

typedef enum __THREADINFOCLASS {
    ThreadBasicInformation,
    ThreadTimes,
    ThreadPriority,
    ThreadBasePriority,
    ThreadAffinityMask,
    ThreadImpersonationToken,
    ThreadDescriptorTableEntry,
    ThreadEnableAlignmentFaultFixup,
    ThreadEventPair_Reusable,
    ThreadQuerySetWin32StartAddress,
    ThreadZeroTlsCell,
    ThreadPerformanceCount,
    ThreadAmILastThread,
    ThreadIdealProcessor,
    ThreadPriorityBoost,
    ThreadSetTlsArrayAddress,
    ThreadIsIoPending,
    ThreadHideFromDebugger,
    ThreadBreakOnTermination,
    ThreadSwitchLegacyState,
    ThreadIsTerminated,
    MaxThreadInfoClass
} THREADINFOCLASS;

typedef NTSTATUS(*NtSetInformationThreadPtr)(HANDLE threadHandle,
    THREADINFOCLASS threadInformationClass,
    PVOID threadInformation,
    ULONG threadInformationLength);

#endif

inline int exit_if_debugger_present()
{
    OBF_BEGIN;

#if MG_OS__WIN_AVAIL
    HMODULE hModule = LoadLibraryW(L"ntdll.dll");
    OBF_IF(hModule == NULL)
        OBF_RETURN(0);
    OBF_ENDIF;
    
    NtSetInformationThreadPtr NtSetInformationThread = (NtSetInformationThreadPtr)GetProcAddress(hModule, "NtSetInformationThread");
    OBF_IF (NtSetInformationThread == NULL)
        OBF_RETURN(0);
    OBF_ENDIF;

    NtSetInformationThread(GetCurrentThread(), ThreadHideFromDebugger, 0, 0);
#endif

    OBF_RETURN(0);
    OBF_END;
}

} // namespace mmbkpp::obf_os::win
} // namespace mmbkpp::obf_os
} // namespace mmbkpp

#endif // !MMBKPP_OBF_OS_WIN_EXIT_IF_DEBUGGER_PRESENT_H_INCLUDED
