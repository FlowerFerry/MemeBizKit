
#ifndef MMBKPP_OBFHW_STRGINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_STRGINFO_HPP_INCLUDED

#include <stdint.h>

#include <string>
#include <memory>
#include <optional>

#include "basicinfo.hpp"
#include <mmbkpp/wrap/obfy/instr.h>

namespace mmbkpp {
namespace obfhw  {
    
    struct strginfo_eval_conds : public basic_eval_conds
    {
            
            constexpr int member_count() const noexcept { return (sizeof(*this) / sizeof(double)) - 1; }
    
            constexpr double total_weight() const noexcept 
            { 
                return sn_weight + total_size_weight; 
            }
    
            constexpr double sn_relative_weight() const noexcept 
            { 
                return sn_weight / total_weight() * member_count();
            }
    
            constexpr double total_size_relative_weight() const noexcept 
            { 
                return total_size_weight / total_weight() * member_count();
            }
    
            double sn_weight = 1.0;
            double total_size_weight = 1.0;
    };

    struct strginfo 
    {
        strginfo() : 
            total_size_(0)
        {}

        constexpr info_type obj_type() const noexcept { return info_type::strg_info; }

        constexpr const std::optional<std::string>&    sn() const noexcept { return sn_; }
        constexpr const std::optional<int64_t>&        total_size() const noexcept { return total_size_; }

        double evaluate(const strginfo& _other, const strginfo_eval_conds& _conds) const {
            OBF_BEGIN;

            double result = 0.0;

            OBF_IF (is_equal(sn_, _other.sn_)) {
                result += _conds.sn_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(total_size_, _other.total_size_)) {
                result += _conds.total_size_relative_weight();
            } OBF_ENDIF;

            result /= _conds.member_count();
            OBF_RETURN(result);
            OBF_END;
        }

        std::optional<std::string> sn_;
        std::optional<int64_t>     total_size_;
    };
    using strginfo_ptr = std::shared_ptr<strginfo>;

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_STRGINFO_HPP_INCLUDED
