
#ifndef MMBKPP_OBFHW_MEMINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_MEMINFO_HPP_INCLUDED

#include <stdint.h>

#include <string>
#include <memory>
#include <optional>

#include "basicinfo.hpp"
#include <mmbkpp/wrap/obfy/instr.h>

namespace mmbkpp {
namespace obfhw  {
    
    struct meminfo_eval_conds : public basic_eval_conds 
    {
            
            constexpr int member_count() const noexcept { return 3; }
    
            constexpr double total_weight() const noexcept 
            { 
                return type_weight + total_size_weight + max_speed_weight; 
            }
    
            constexpr double type_relative_weight() const noexcept 
            { 
                return type_weight / total_weight() * member_count();
            }
    
            constexpr double total_size_relative_weight() const noexcept 
            { 
                return total_size_weight / total_weight() * member_count();
            }
    
            constexpr double max_speed_relative_weight() const noexcept 
            { 
                return max_speed_weight / total_weight() * member_count();
            }
    
            int8_t type_weight = 100;
            int8_t total_size_weight = 100;
            int8_t max_speed_weight = 100;
    };

    struct meminfo 
    {
        meminfo()
        {}
        
        constexpr info_type obj_type() const noexcept { return info_type::mem_info; }

        constexpr const std::optional<int>&            type() const noexcept { return type_; }
        constexpr const std::optional<int64_t>&        total_size() const noexcept { return total_size_; }
        constexpr const std::optional<int64_t>&        max_speed() const noexcept { return max_speed_; }

        double evaluate(const meminfo& _other, const meminfo_eval_conds& _conds) const {
            OBF_BEGIN

            double result = 0.0;
            OBF_IF (is_equal(type_, _other.type_)) {
                result += _conds.type_relative_weight();
            } OBF_ENDIF;
            
            OBF_IF (is_equal(total_size_, _other.total_size_)) {
                result += _conds.total_size_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(max_speed_, _other.max_speed_)) {
                result += _conds.max_speed_relative_weight();
            } OBF_ENDIF;

            result /= _conds.member_count();
            OBF_RETURN(result);
            OBF_END
        }

        std::optional<int>     type_;
        std::optional<int64_t> total_size_;
        std::optional<int64_t> max_speed_;
    };
    using meminfo_ptr = std::shared_ptr<meminfo>;

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_MEMINFO_HPP_INCLUDED
