
#ifndef MMBKPP_OBF_HW_MMC_FROM_YYJSON_H_INCLUDED
#define MMBKPP_OBF_HW_MMC_FROM_YYJSON_H_INCLUDED

#include <yyjson.h>

#include <megopp/err/err.h>
#include <mmbkpp/obf/hardware/mmc.h>
#include <mmbkpp/wrap/obfy/instr.h>
#include <memepp/convert/yyjson.hpp>

namespace mmbkpp {
namespace obf_hw {

inline mgpp::err mmc_from_yyjson(yyjson_val* _val, mmc_info& _info)
{
    OBF_BEGIN;

    mgpp::err e;

    OBF_IF(_val == nullptr) {
        e = mgpp::err{ MGEC__ERR };
        OBF_RETURN(e);
    }
    OBF_ENDIF;

    auto dev_name = memepp::from_yyjson_value(_val, "dev_name", {});
    OBF_IF(dev_name.empty()) {
        e = mgpp::err{ MGEC__ERR };
        OBF_RETURN(e);
    }
    OBF_ENDIF;

    auto cid = memepp::from_yyjson_value(_val, "cid", {});
    OBF_IF(cid.empty()) {
        e = mgpp::err{ MGEC__ERR };
        OBF_RETURN(e);
    }
    OBF_ENDIF;

    _info.dev_name  = dev_name;
    _info.cid       = cid;
    _info.csd       = memepp::from_yyjson_value(_val, "csd", {});
    _info.oemid     = memepp::from_yyjson_value(_val, "oemid", {});
    _info.name      = memepp::from_yyjson_value(_val, "name", {});
    _info.serial    = memepp::from_yyjson_value(_val, "serial", {});
    _info.manfid    = memepp::from_yyjson_value(_val, "manfid", {});
    _info.date      = memepp::from_yyjson_value(_val, "date", {});
    _info.type      = memepp::from_yyjson_value(_val, "type", {});
    _info.removable = removable_t::unknown;
    auto removable  = memepp::from_yyjson_value(_val, "removable", {});
    OBF_IF (removable == "fixed") {
        _info.removable = removable_t::fixed;
    }
    OBF_ENDIF; 
    OBF_IF (removable == "removable") {
        _info.removable = removable_t::removable;
    } 
    OBF_ENDIF;

    OBF_RETURN(e);
    OBF_END;
}

}
}

#endif // !MMBKPP_OBF_HW_MMC_FROM_YYJSON_H_INCLUDED
