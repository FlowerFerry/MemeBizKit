
#ifndef MMBKPP_OBFHW_ALLWINNER_INTO_YYJSON_H_INCLUDED
#define MMBKPP_OBFHW_ALLWINNER_INTO_YYJSON_H_INCLUDED

#include <yyjson.h>

#include <megopp/err/err.h>
#include <mmbkpp/obf/hardware/allwinner.h>
#include <mmbkpp/wrap/obfy/instr.h>

namespace mmbkpp {
namespace obf_hw {

inline mgpp::err allwinner_into_yyjson(
    const allwinner_info& _info, yyjson_mut_doc* _doc, bool _copy,
    yyjson_mut_val* _val = NULL, const char* _key = NULL, yyjson_mut_val** _out = NULL)
{
    OBF_BEGIN;

    mgpp::err e;

    OBF_IF (_val == NULL) {
        auto yyroot = yyjson_mut_doc_get_root(_doc);
        OBF_IF (yyroot == NULL) {
            yyroot = yyjson_mut_obj(_doc);
            yyjson_mut_doc_set_root(_doc, yyroot);
        }
        OBF_ENDIF;

        _val = yyroot;
    }
    OBF_ENDIF;

    auto create_fn = [](const allwinner_info& _info, yyjson_mut_doc* _doc, bool _copy)
    {
        auto strn_fn = (_copy ? yyjson_mut_obj_add_strncpy : yyjson_mut_obj_add_strn);

        auto yyinfo = yyjson_mut_obj(_doc);

        strn_fn(_doc, yyinfo, "serial",
            _info.serial.data(), static_cast<size_t>(_info.serial.size()));

        return yyinfo;
    };

    OBF_IF (yyjson_mut_is_obj(_val))
    {
        OBF_IF (_key == NULL) {
            e = mgpp::err{ MGEC__INVAL };
            OBF_RETURN(e);
        }
        OBF_ENDIF;

        auto yyinfo = create_fn(_info, _doc, _copy);
        yyjson_mut_obj_add_val(_doc, _val, _key, yyinfo);

        OBF_IF(_out != NULL) {
            *_out = yyinfo;
        }
        OBF_ENDIF;
        e = mgpp::err{ };
        OBF_RETURN(e);
    }
    OBF_ENDIF;
    OBF_IF (yyjson_mut_is_arr(_val))
    {
        auto yyinfo = create_fn(_info, _doc, _copy);
        yyjson_mut_arr_add_val(_val, yyinfo);

        OBF_IF(_out != NULL) {
            *_out = yyinfo;
        }
        OBF_ENDIF;
        e = mgpp::err{ };
        OBF_RETURN(e);
    }
    OBF_ENDIF;

    e = mgpp::err{ MGEC__OPNOTSUPP };
    OBF_RETURN(e);
    OBF_END;
}

}
}

#endif // !MMBKPP_OBFHW_ALLWINNER_INTO_YYJSON_H_INCLUDED
