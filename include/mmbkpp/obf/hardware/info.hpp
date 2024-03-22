
#ifndef MMBKPP_OBFHW_INFO_HPP_INCLUDED
#define MMBKPP_OBFHW_INFO_HPP_INCLUDED

#include "basicinfo.hpp"
#include "cpuinfo.hpp"
#include "meminfo.hpp"
#include "strginfo.hpp"
#include "sysinfo.hpp"

#include <map>
#include <vector>

#include <mmbkpp/wrap/obfy/instr.h>
#include "../../thirdparty/Obfuscate/obfuscate.h"

namespace mmbkpp {
namespace obfhw  {
    
    struct info_eval_conds
    {
        static constexpr double lost_penalty = -0.1;

        double total_weight() const noexcept
        {
            OBF_BEGIN
            double total = 0.0;
            for (const auto& [key, value] : weights)
            {
                total += value;
            }
            OBF_RETURN(total);
            OBF_END
        }

        double relative_weight(info_type _type) const noexcept
        {
            OBF_BEGIN
            auto it = weights.find(_type);
            OBF_IF (it != weights.end())
            {
                auto result = it->second / total_weight() * weights.size();
                OBF_RETURN(result);
            } OBF_ENDIF;
            
            OBF_RETURN(0.0);
            OBF_END
        }

        std::map<info_type, double> weights;
        std::map<info_type, std::unique_ptr<basic_eval_conds>> eval_conds;
    };

    struct info 
    {
        info()
        {}
    
        double evaluate(const info& _other, const info_eval_conds& _conds) const;

        sysinfo_ptr               sysinfo_;
        std::vector<cpuinfo_ptr>  cpuinfo_list_;
        std::vector<meminfo_ptr>  meminfo_list_;
        std::vector<strginfo_ptr> strginfo_list_;
    };

    inline double info::evaluate(const info& _other, const info_eval_conds& _conds) const
    {
        OBF_BEGIN
        double result = 0.0;
        size_t count  = 0;

        if (sysinfo_ && _other.sysinfo_)
        {
            sysinfo_eval_conds conds;
            auto condit = _conds.eval_conds.find(info_type::sys_info);
            if (condit != _conds.eval_conds.end() && condit->second)
                conds = *static_cast<sysinfo_eval_conds*>(condit->second.get());

            result += sysinfo_->evaluate(*_other.sysinfo_, conds);
            ++count;
        }
        else if (sysinfo_ || _other.sysinfo_)
        {
            result += info_eval_conds::lost_penalty;
        }

        cpuinfo_eval_conds cpuinfo_conds;
        auto condit = _conds.eval_conds.find(info_type::processor_info);
        if (condit != _conds.eval_conds.end() && condit->second)
            cpuinfo_conds = *static_cast<cpuinfo_eval_conds*>(condit->second.get());

        for (size_t idx = 0, n = (std::min)(cpuinfo_list_.size(), _other.cpuinfo_list_.size()); idx < n; ++idx)
        {
            result += cpuinfo_list_[idx]->evaluate(*_other.cpuinfo_list_[idx], cpuinfo_conds);
            ++count;
        }

        meminfo_eval_conds meminfo_conds;
        condit = _conds.eval_conds.find(info_type::mem_info);
        if (condit != _conds.eval_conds.end() && condit->second)
            meminfo_conds = *static_cast<meminfo_eval_conds*>(condit->second.get());

        for (size_t idx = 0, n = (std::min)(meminfo_list_.size(), _other.meminfo_list_.size()); idx < n; ++idx)
        {
            result += meminfo_list_[idx]->evaluate(*_other.meminfo_list_[idx], meminfo_conds);
            ++count;
        }

        strginfo_eval_conds strginfo_conds;
        condit = _conds.eval_conds.find(info_type::strg_info);
        if (condit != _conds.eval_conds.end() && condit->second)
            strginfo_conds = *static_cast<strginfo_eval_conds*>(condit->second.get());

        for (size_t idx = 0, n = (std::min)(strginfo_list_.size(), _other.strginfo_list_.size()); idx < n; ++idx)
        {
            result += strginfo_list_[idx]->evaluate(*_other.strginfo_list_[idx], strginfo_conds);
            ++count;
        }

        OBF_IF (cpuinfo_list_.size() != _other.cpuinfo_list_.size())
        {
            result += (info_eval_conds::lost_penalty * 
                abs(ptrdiff_t(cpuinfo_list_.size()) - ptrdiff_t(_other.cpuinfo_list_.size())));
        } OBF_ENDIF;

        OBF_IF (meminfo_list_.size() != _other.meminfo_list_.size())
        {
            result += (info_eval_conds::lost_penalty * 
                abs(ptrdiff_t(meminfo_list_.size()) - ptrdiff_t(_other.meminfo_list_.size())));
        } OBF_ENDIF;

        OBF_IF (strginfo_list_.size() != _other.strginfo_list_.size())
        {
            result += (info_eval_conds::lost_penalty * 
                abs(ptrdiff_t(strginfo_list_.size()) - ptrdiff_t(_other.strginfo_list_.size())));
        } OBF_ENDIF;

        result /= count;
        OBF_RETURN(result);
        OBF_END
    }

}
}

#endif // !MMBKPP_OBFHW_INFO_HPP_INCLUDED
