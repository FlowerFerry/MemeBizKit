
#ifndef MMBKPP_OBF_OS_LINUX_DIR_H_INCLUDED
#define MMBKPP_OBF_OS_LINUX_DIR_H_INCLUDED

#include <mego/predef/os/linux.h>
#include <mego/util/converted_native_string.h>

#include <megopp/err/err.h>
#include <memepp/string_view.hpp>
#include <megopp/util/scope_cleanup.h>
#include <mmbkpp/wrap/obfy/instr.h>

#if MG_OS__LINUX_AVAIL
#include <dirent.h>
#endif

namespace mmbkpp {
namespace obf_linux {

    template <typename _Fn>
    inline mgpp::err readdir(const char* _path, mmint_t _slen, _Fn&& _fn)
    {  
        OBF_BEGIN;

#if MG_OS__LINUX_AVAIL
        static_assert(std::is_invocable_v<_Fn, const dirent*>, "Invalid function type");

        mmn_char_cptr_t path = NULL;
        mmint_t path_len = 0;
        mgec_t ec = mgu__to_cns(_path, _slen, &path, &path_len, 0);
        OBF_IF (MEGO_SYMBOL__UNLIKELY(ec != 0))
            OBF_RETURN(mgpp::err{ ec });
        OBF_ENDIF;
        MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { mgu__free_cns(_path, path); });

        struct dirent* entry = NULL;
        DIR* dir = ::opendir(path);
        OBF_IF (MEGO_SYMBOL__UNLIKELY(dir == NULL))
            OBF_RETURN(mgpp::err{ MGEC__ERR });
        OBF_ENDIF;
        MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { ::closedir(dir); });

        OBF_WHILE ((entry = ::readdir(dir)) != NULL)
        {
            if constexpr (std::is_same_v<std::invoke_result_t<_Fn, const dirent*>, bool>)
            {
                OBF_IF (!_fn(entry))
                    OBF_BREAK;
                OBF_ENDIF;
            }
            else {
                _fn(entry);
            }
        } OBF_ENDWHILE;

        OBF_RETURN(mgpp::err{ 0 });

#endif
        OBF_RETURN(mgpp::err{ MGEC__OPNOTSUPP });
        OBF_END;
    }
    
}
}

#endif // !MMBKPP_OBF_OS_LINUX_DIR_H_INCLUDED
