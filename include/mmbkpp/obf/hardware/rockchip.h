
#ifndef MMBKPP_OBFHW_ROCKCHIP_H_INCLUDED
#define MMBKPP_OBFHW_ROCKCHIP_H_INCLUDED

#include <memepp/string.hpp>
#include <megopp/err/err.h>
#include <megopp/util/scope_cleanup.h>
#include <mego/util/std/file.h>
#include <mmbkpp/wrap/obfy/instr.h>

#include <cstdio>

namespace mmbkpp {
namespace obf_hw {

    struct rockchip_info
    {
        rockchip_info() = default;
        memepp::string serial;  // 24 char lowercase hex, 12 unique bytes of the CPU ID
    };

    inline bool rockchip__read_file(const char* _path, char* _buf, size_t _cap, size_t& _len)
    {
        FILE* fp = mgu_fopen(_path, -1, "rb", -1);
        if (!fp) {
            return false;
        }
        MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { fclose(fp); });
        _len = fread(_buf, 1, _cap - 1, fp);
        return (_len > 0);
    }

    // Reads the Rockchip SoC CPU ID from the OTP nvmem cell.
    // Source: /sys/bus/nvmem/devices/rockchip-otp0/nvmem, from byte offset 0x00.
    //   bytes 0..4   = chip model (e.g. "RK5\x88"), skipped
    //   bytes 4..16  = per-unit unique serial (12 bytes, hex-encoded here)
    // Returns 0 on success (the 12-byte serial is not all zero).
    inline mgpp::err read_rockchip(memepp::string& _serial)
    {
        OBF_BEGIN;
        mgpp::err e;

#if MG_OS__LINUX_AVAIL
        constexpr size_t kCpuIdOffset = 0x00;
        constexpr size_t kCpuIdLen    = 16;
        constexpr size_t kSerialSkip  = 4;
        constexpr size_t kSerialLen   = kCpuIdLen - kSerialSkip;  // 12

        char buf[64];
        char out[kSerialLen * 2 + 1];  // 12-byte serial -> 24 hex chars + NUL
        size_t len = 0;
        OBF_IF (rockchip__read_file("/sys/bus/nvmem/devices/rockchip-otp0/nvmem", buf, sizeof(buf), len))
        {
            OBF_IF (len >= kCpuIdOffset + kCpuIdLen)
            {
                const char* src = buf + kCpuIdOffset + kSerialSkip;   // the 12 unique bytes
                bool all_zero = true;
                size_t i = 0;
                OBF_FOR (i = 0, i < kSerialLen, ++i)
                {
                    OBF_IF (static_cast<unsigned char>(src[i]) != 0)
                    {
                        all_zero = false;
                        OBF_BREAK;
                    }
                    OBF_ENDIF;
                }
                OBF_ENDFOR;

                OBF_IF (all_zero == false)
                {
                    static const char* const hexd = "0123456789abcdef";
                    OBF_FOR (i = 0, i < kSerialLen, ++i)
                    {
                        const unsigned char b = static_cast<unsigned char>(src[i]);
                        out[i * 2]     = hexd[b >> 4];
                        out[i * 2 + 1] = hexd[b & 0x0f];
                    }
                    OBF_ENDFOR;
                    out[kSerialLen * 2] = '\0';
                    _serial = memepp::string{ out, static_cast<memepp::string::size_type>(kSerialLen * 2) };
                    OBF_RETURN(e);
                }
                OBF_ENDIF;
            }
            OBF_ENDIF;
        }
        OBF_ENDIF;

        OBF_RETURN(e);
#else
        OBF_RETURN(mgpp::err{ MGEC__OPNOTSUPP });
#endif
        OBF_END;
    }

}} // namespace mmbkpp::obf_hw

#endif // !MMBKPP_OBFHW_ROCKCHIP_H_INCLUDED
