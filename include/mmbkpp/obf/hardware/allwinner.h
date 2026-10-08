
#ifndef MMBKPP_OBFHW_ALLWINNER_H_INCLUDED
#define MMBKPP_OBFHW_ALLWINNER_H_INCLUDED

#include <memepp/string.hpp>
#include <megopp/err/err.h>
#include <megopp/util/scope_cleanup.h>
#include <mego/util/std/file.h>
#include <mmbkpp/wrap/obfy/instr.h>

#include <cstdio>
#include <cstring>

namespace mmbkpp {
namespace obf_hw {

    struct allwinner_info
    {
        allwinner_info() = default;
        memepp::string serial;  // 16..32 char hex eFuse serial (sunxi_serial / chipid)
    };

    // Validates a SoC serial: 8..32 hex chars, not all the same nibble
    // (an un-programmed eFuse reads back as 0000... or FFFF...).
    inline bool is_valid_hwid_hex(const char* _s, size_t _len)
    {
        if (_len < 8 || _len > 32) {
            return false;
        }
        for (size_t i = 0; i < _len; ++i) {
            const char c = _s[i];
            if (!(((c >= '0') && (c <= '9')) ||
                  ((c >= 'a') && (c <= 'f')) ||
                  ((c >= 'A') && (c <= 'F')))) {
                return false;
            }
        }
        for (size_t i = 1; i < _len; ++i) {
            if (_s[i] != _s[0]) {
                return true;
            }
        }
        return false;
    }

    inline bool allwinner__read_file(const char* _path, char* _buf, size_t _cap, size_t& _len)
    {
        FILE* fp = mgu_fopen(_path, -1, "rb", -1);
        if (!fp) {
            return false;
        }
        MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { fclose(fp); });
        _len = fread(_buf, 1, _cap - 1, fp);
        return (_len > 0);
    }

    // Reads the Allwinner (sunxi) SoC serial from eFuse.
    // Primary: parse "sunxi_serial: <hex>" from /sys/class/sunxi_info/sys_info.
    // Fallback: write "chipid" to /sys/class/sunxi_info/key_info then read it back.
    // Returns 0 on success (serial is a 16..32 char hex that is not all the same nibble).
    inline mgpp::err read_allwinner(memepp::string& _serial)
    {
        OBF_BEGIN;

        mgpp::err e;

#if MG_OS__LINUX_AVAIL
        char buf[256];
        size_t len = 0;

        OBF_IF (allwinner__read_file("/sys/class/sunxi_info/sys_info", buf, sizeof(buf), len))
        {
            buf[len] = '\0';
            const char* p = strstr(buf, "sunxi_serial");
            OBF_IF (p != nullptr)
            {
                p = strchr(p, ':');
                OBF_IF (p != nullptr)
                {
                    ++p;
                    while (*p == ' ' || *p == '\t') {
                        ++p;
                    }
                    const char* end = p;
                    while (*end && *end != '\r' && *end != '\n') {
                        ++end;
                    }
                    OBF_IF (is_valid_hwid_hex(p, static_cast<size_t>(end - p)))
                    {
                        _serial = memepp::string{ p, static_cast<memepp::string::size_type>(end - p) };
                        OBF_RETURN(e);
                    }
                    OBF_ENDIF;
                }
                OBF_ENDIF;
            }
            OBF_ENDIF;
        }
        OBF_ENDIF;

        FILE* fp = mgu_fopen("/sys/class/sunxi_info/key_info", -1, "r+", -1);
        OBF_IF (fp == nullptr) {
            OBF_RETURN(e);
        }
        OBF_ENDIF;
        MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { if (fp) fclose(fp); });

        OBF_IF (fwrite("chipid", 1, 6, fp) != 6) {
            OBF_RETURN(e);
        }
        OBF_ENDIF;
        OBF_IF (fseek(fp, 0, SEEK_SET) != 0) {
            OBF_RETURN(e);
        }
        OBF_ENDIF;
        len = fread(buf, 1, sizeof(buf) - 1, fp);
        OBF_IF (len == 0) {
            OBF_RETURN(e);
        }
        OBF_ENDIF;
        buf[len] = '\0';
        const char* p = buf;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        const char* end = p;
        while (*end && *end != '\r' && *end != '\n') {
            ++end;
        }
        OBF_IF (is_valid_hwid_hex(p, static_cast<size_t>(end - p)) == false) {
            OBF_RETURN(e);
        }
        OBF_ENDIF;

        _serial = memepp::string{ p, static_cast<memepp::string::size_type>(end - p) };
        OBF_RETURN(e);
#else
        OBF_RETURN(mgpp::err{ MGEC__OPNOTSUPP });
#endif
        OBF_END;
    }

}} // namespace mmbkpp::obf_hw

#endif // !MMBKPP_OBFHW_ALLWINNER_H_INCLUDED
