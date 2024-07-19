
#ifndef MMBKPP_OBF_SW_IS_DEBUGGER_PRESENT_H_INCLUDED
#define MMBKPP_OBF_SW_IS_DEBUGGER_PRESENT_H_INCLUDED

#include <mego/util/os/windows/windows_simplify.h>
#include <mego/predef/os/linux.h>

#if !MG_OS__WIN_AVAIL
#include <sys/ptrace.h>
#endif

#include <mmbkpp/wrap/obfy/instr.h>

#include <iostream>
#include <string>

namespace mmbkpp {
namespace obf_sw {

inline bool is_debugger_present()
{
#if MG_OS__WIN_AVAIL
    return ::IsDebuggerPresent();
#elif MG_OS__LINUX_AVAIL
    OBF_BEGIN;

    std::ifstream file("/proc/self/status");
    std::string line;
    OBF_WHILE(std::getline(file, line))
    {
        OBF_IF(line.find("TracerPid:") != std::string::npos)
        {
            auto v = atol(line.substr(11).data());
            OBF_IF(v != 0)
                OBF_RETURN(true);
            OBF_ENDIF
        }
        OBF_ENDIF
    }
    OBF_ENDWHILE;

    errno = 0;
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    OBF_IF(errno == 0) {
        ptrace(PTRACE_DETACH, 0, NULL, NULL);
        OBF_RETURN(false);
    }
    OBF_ELSE {
        OBF_RETURN(true);
    }
    OBF_ENDIF

    OBF_RETURN(false);
    OBF_END;
#else
    OBF_BEGIN;
    errno = 0;
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    OBF_IF(errno == 0) {
        ptrace(PTRACE_DETACH, 0, NULL, NULL);
        OBF_RETURN(false);
    }
    OBF_ELSE {
        OBF_RETURN(true);
    }
    OBF_ENDIF
    
    OBF_RETURN(false);
    OBF_END;
#endif
}

}
}

#endif // !MMBKPP_OBF_SW_IS_DEBUGGER_PRESENT_H_INCLUDED
