
#ifndef MMBKPP_OBFHW_MMC_H_INCLUDED
#define MMBKPP_OBFHW_MMC_H_INCLUDED

#include <memepp/string.hpp>
#include <memepp/string_view.hpp>
#include <megopp/util/scope_cleanup.h>
#include <mmbkpp/wrap/obfy/instr.h>
#include <mmbkpp/obf/os/linux/dir.h>
#include <megopp/err/err.h>

namespace mmbkpp {
namespace obfhw {

    enum class removable_t
    {
        unknown,
        fixed,
        removable
    };

    struct mmc_info
    {
        mmc_info()
            : removable(removable_t::unknown)
        {
        }
        
        inline void reset()
        {
            dev_name.reset();
            cid.reset();
            csd.reset();
            oemid.reset();
            name.reset();
            serial.reset();
            manfid.reset();
            date.reset();
            type.reset();
            removable = removable_t::unknown;
        }

        memepp::string dev_name;
        memepp::string cid;
        memepp::string csd;
        memepp::string oemid;
        memepp::string name;
        memepp::string serial;
        memepp::string manfid;
        memepp::string date;
        memepp::string type;
        removable_t    removable;
    };

    inline mgpp::err get_mmc_info(
        const memepp::string_view& _dev_name, mmc_info& _info)
    {
        OBF_BEGIN;

        auto dev_name = _dev_name.to_string();
#if MG_OS__LINUX_AVAIL
        FILE* fp = NULL;

        size_t len = 0;
        char buf[512];
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/sys/block/%s/device/cid", dev_name.data());
        fp = fopen(path, "r");
        MEGOPP_UTIL__ON_SCOPE_CLEANUP([&fp] { if (fp) fclose(fp); });
        OBF_IF (fp == NULL) {
            OBF_RETURN(mgpp::err{ MGEC__ERR });
        } OBF_ENDIF;
        
        len = fread(buf, 1, sizeof(buf), fp);
        OBF_IF (len == 0) {
            OBF_RETURN(mgpp::err{ MGEC__ERR });
        } OBF_ENDIF;
        OBF_IF (buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        } OBF_ENDIF;

        _info.dev_name = dev_name;
        _info.cid = buf;

        snprintf(path, sizeof(path), "/sys/block/%s/device/csd", dev_name.data());
        fp = fopen(path, "r");
        OBF_IF (fp != NULL) {
            len = fread(buf, 1, sizeof(buf), fp);
            OBF_IF (len != 0) {
                if (buf[len - 1] == '\n') {
                    buf[len - 1] = '\0';
                }
                _info.csd = buf;
            } OBF_ENDIF;
        } OBF_ENDIF;

        snprintf(path, sizeof(path), "/sys/block/%s/device/oemid", dev_name.data());
        fp = fopen(path, "r");
        OBF_IF (fp != NULL) {
            len = fread(buf, 1, sizeof(buf), fp);
            OBF_IF (len != 0) {
                if (buf[len - 1] == '\n') {
                    buf[len - 1] = '\0';
                }
                _info.oemid = buf;
            } OBF_ENDIF;
        } OBF_ENDIF;

        snprintf(path, sizeof(path), "/sys/block/%s/device/name", dev_name.data());
        fp = fopen(path, "r");
        OBF_IF (fp != NULL) {
            len = fread(buf, 1, sizeof(buf), fp);
            OBF_IF (len != 0) {
                if (buf[len - 1] == '\n') {
                    buf[len - 1] = '\0';
                }
                _info.name = buf;
            } OBF_ENDIF;
        } OBF_ENDIF;

        snprintf(path, sizeof(path), "/sys/block/%s/device/serial", dev_name.data());
        fp = fopen(path, "r");
        OBF_IF (fp != NULL) {
            len = fread(buf, 1, sizeof(buf), fp);
            OBF_IF (len != 0) {
                if (buf[len - 1] == '\n') {
                    buf[len - 1] = '\0';
                }
                _info.serial = buf;
            } OBF_ENDIF;
        } OBF_ENDIF;

        snprintf(path, sizeof(path), "/sys/block/%s/device/manfid", dev_name.data());
        fp = fopen(path, "r");
        OBF_IF (fp != NULL) {
            len = fread(buf, 1, sizeof(buf), fp);
            OBF_IF (len != 0) {
                if (buf[len - 1] == '\n') {
                    buf[len - 1] = '\0';
                }
                _info.manfid = buf;
            } OBF_ENDIF;
        } OBF_ENDIF;

        snprintf(path, sizeof(path), "/sys/block/%s/device/date", dev_name.data());
        fp = fopen(path, "r");
        OBF_IF (fp != NULL) {
            len = fread(buf, 1, sizeof(buf), fp);
            OBF_IF (len != 0) {
                if (buf[len - 1] == '\n') {
                    buf[len - 1] = '\0';
                }
                _info.date = buf;
            } OBF_ENDIF;
        } OBF_ENDIF;

        snprintf(path, sizeof(path), "/sys/block/%s/device/type", dev_name.data());
        fp = fopen(path, "r");
        OBF_IF (fp != NULL) {
            len = fread(buf, 1, sizeof(buf), fp);
            OBF_IF (len != 0) {
                if (buf[len - 1] == '\n') {
                    buf[len - 1] = '\0';
                }
                _info.type = buf;
            } OBF_ENDIF;
        } OBF_ENDIF;

        snprintf(path, sizeof(path), "/sys/block/%s/device/removable", dev_name.data());
        fp = fopen(path, "r");
        OBF_IF (fp != NULL) {
            len = fread(buf, 1, sizeof(buf), fp);
            OBF_IF (len != 0) {
                if (buf[len - 1] == '\n') {
                    buf[len - 1] = '\0';
                }
                _info.removable = (buf[0] == '1') ? removable_t::removable : removable_t::fixed;
            } OBF_ENDIF;
        }
        OBF_ELSE {
            _info.removable = removable_t::unknown;
        } OBF_ENDIF;

        OBF_RETURN(mgpp::err{ 0 });

#endif
        OBF_RETURN(mgpp::err{ MGEC__OPNOTSUPP });
        OBF_END;
    }

    template<typename _C>
    inline mgpp::err enum_mmcs(std::back_insert_iterator<_C> _it)
    {
        OBF_BEGIN;

#if MG_OS__LINUX_AVAIL
        auto e = mmbkpp::obf_linux::readdir("/sys/block", -1, 
        [&](const dirent* _entry) {
            if (_entry->d_name[0] == 'm' 
             && _entry->d_name[1] == 'm'
             && _entry->d_name[2] == 'c') 
            {
                mmc_info info;
                auto err = get_mmc_info(_entry->d_name, info);
                if ( err ) {
                    return ;
                }
                *_it = info;
            }
        });

        OBF_RETURN(mgpp::err{ 0 });
#endif 

        OBF_RETURN(mgpp::err{ MGEC__OPNOTSUPP });
        OBF_END;
    }

}} // namespace mmbkpp::obfhw

#endif // !MMBKPP_OBFHW_MMC_H_INCLUDED
