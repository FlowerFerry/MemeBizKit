
#ifndef MMBKPP_OBFHW_INFO_HPP_INCLUDED
#define MMBKPP_OBFHW_INFO_HPP_INCLUDED

#include "basicinfo.hpp"
#include "cpuinfo.hpp"
#include "meminfo.hpp"
#include "strginfo.hpp"
#include "sysinfo.hpp"

#include <map>
#include <vector>

#include "../../3rdparty/obfy/instr.h"
#include "../../3rdparty/Obfuscate/obfuscate.h"

namespace mmbkpp {
namespace obfhw {
    
    struct match_conds
    {
        // std::map<info_type, double>;
    };

    struct info 
    {
        info()
        {}
    
        double match_score(const info& _other, const match_conds& _conds) const;

        sysinfo_ptr                 sysinfo_;
        std::vector<cpuinfo_ptr>    cpuinfo_list_;
        std::vector<meminfo_ptr>    meminfo_list_;
        std::vector<strginfo_ptr>   strginfo_list_;
    };

    inline double info::match_score(const info& _other, const match_conds& _conds) const
    {
        OBF_BEGIN


        RETURN(0.0);
        OBF_END
    }

}
}

#endif // !MMBKPP_OBFHW_INFO_HPP_INCLUDED
