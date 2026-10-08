
#ifndef MMBKPP_OBFHW_ROCKCHIP_FROM_YYJSON_H_INCLUDED
#define MMBKPP_OBFHW_ROCKCHIP_FROM_YYJSON_H_INCLUDED

#include <yyjson.h>

#include <megopp/err/err.h>
#include <mmbkpp/obf/hardware/rockchip.h>
#include <mmbkpp/wrap/obfy/instr.h>
#include <memepp/convert/yyjson.hpp>

namespace mmbkpp {
namespace obf_hw {

inline mgpp::err rockchip_from_yyjson(yyjson_val* _val, rockchip_info& _info)
{
    OBF_BEGIN;

    mgpp::err e;

    OBF_IF(_val == nullptr) {
        e = mgpp::err{ MGEC__ERR };
        OBF_RETURN(e);
    }
    OBF_ENDIF;

    auto serial = memepp::from_yyjson_value(_val, "serial", {});
    OBF_IF(serial.empty()) {
        e = mgpp::err{ MGEC__ERR };
        OBF_RETURN(e);
    }
    OBF_ENDIF;

    _info.serial = serial;

    OBF_RETURN(e);
    OBF_END;
}

}
}

#endif // !MMBKPP_OBFHW_ROCKCHIP_FROM_YYJSON_H_INCLUDED
