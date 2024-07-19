
#ifndef MMBKPP_OBFHW_MMC_INTO_YYJSON_H_INCLUDED
#define MMBKPP_OBFHW_MMC_INTO_YYJSON_H_INCLUDED

#include <yyjson.h>

#include <megopp/err/err.h>
#include <mmbkpp/obf/hardware/mmc.h>
#include <mmbkpp/wrap/obfy/instr.h>

namespace mmbkpp {
namespace obf_hw {

inline mgpp::err mmc_into_yyjson(
    const mmc_info& _info, yyjson_mut_doc* _doc, bool _copy,
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

    auto create_fn = [](const mmc_info& _info, yyjson_mut_doc* _doc, bool _copy) 
    {
        auto strn_fn = (_copy ? yyjson_mut_obj_add_strncpy : yyjson_mut_obj_add_strn);
        auto str_fn  = (_copy ? yyjson_mut_arr_add_strcpy  : yyjson_mut_arr_add_str);

        auto yyinfo = yyjson_mut_obj(_doc);

        strn_fn(_doc, yyinfo, "dev_name", 
            _info.dev_name.data(), static_cast<size_t>(_info.dev_name.size()));
        strn_fn(_doc, yyinfo, "cid", 
            _info.cid.data(), static_cast<size_t>(_info.cid.size()));
        strn_fn(_doc, yyinfo, "csd", 
            _info.csd.data(), static_cast<size_t>(_info.csd.size()));
        strn_fn(_doc, yyinfo, "oemid", 
            _info.oemid.data(), static_cast<size_t>(_info.oemid.size()));
        strn_fn(_doc, yyinfo, "name", 
            _info.name.data(), static_cast<size_t>(_info.name.size()));
        strn_fn(_doc, yyinfo, "serial", 
            _info.serial.data(), static_cast<size_t>(_info.serial.size()));
        strn_fn(_doc, yyinfo, "manfid", 
            _info.manfid.data(), static_cast<size_t>(_info.manfid.size()));
        strn_fn(_doc, yyinfo, "date", 
            _info.date.data(), static_cast<size_t>(_info.date.size()));
        strn_fn(_doc, yyinfo, "type", 
            _info.type.data(), static_cast<size_t>(_info.type.size()));
        str_fn (_doc, yyinfo, "removable", 
            _info.removable == removable_t::fixed ? "fixed" : 
            _info.removable == removable_t::removable ? "removable" : "unknown");
        
        return yyinfo;
    };

    OBF_IF (yyjson_mut_is_obj(_val)) 
    {
        OBF_IF (_key == NULL) {
            e = mgpp::err{ MGEC__ERR };
            OBF_RETURN(e);
        }
        OBF_ENDIF;

        auto yyinfo = create_fn(_info, _doc);
        yyjson_mut_obj_add_val(_doc, _val, _key, yyinfo);

        OBF_IF(_out != NULL) {
            *_out = yyinfo;
        }
        OBF_ENDIF;
        e = mgpp::err{ MGEC__OK };
        OBF_RETURN(e);
    }
    OBF_ENDIF;
    OBF_IF (yyjson_mut_is_arr(_val)) 
    {
        auto yyinfo = create_fn(_info, _doc);
        yyjson_mut_arr_add_val(_val, yyinfo);

        OBF_IF(_out != NULL) {
            *_out = yyinfo;
        }
        OBF_ENDIF;
        e = mgpp::err{ MGEC__OK };
        OBF_RETURN(e);
    }
    OBF_ENDIF;

    e = mgpp::err{ MGEC__ERR };
    OBF_RETURN(e);
    OBF_END;
}

}    
}

#endif // !MMBKPP_OBFHW_MMC_INTO_YYJSON_H_INCLUDED
