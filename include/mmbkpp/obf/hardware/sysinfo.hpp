
#ifndef MMBKPP_OBFHW_SYSINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_SYSINFO_HPP_INCLUDED

#include <stdint.h>

#include <string>
#include <memory>
#include <optional>

#include "basicinfo.hpp"
#include <mmbkpp/wrap/obfy/instr.h>

namespace mmbkpp {
namespace obfhw  {
    
    struct sysinfo_eval_conds : public basic_eval_conds
    {
            
            constexpr int member_count() const noexcept { return 4; }
    
            constexpr double total_weight() const noexcept 
            { 
                return manufacturer_weight + product_name_weight + sn_weight + uuid_weight; 
            }
    
            constexpr double manufacturer_relative_weight() const noexcept 
            { 
                return manufacturer_weight / total_weight() * member_count();
            }
    
            constexpr double product_name_relative_weight() const noexcept 
            { 
                return product_name_weight / total_weight() * member_count();
            }
    
            constexpr double sn_relative_weight() const noexcept 
            { 
                return sn_weight / total_weight() * member_count();
            }
    
            constexpr double uuid_relative_weight() const noexcept 
            { 
                return uuid_weight / total_weight() * member_count();
            }
    
            int8_t manufacturer_weight = 100;
            int8_t product_name_weight = 100;
            int8_t sn_weight = 100;
            int8_t uuid_weight = 100;
    };

    struct sysinfo 
    {
        sysinfo()
        {}
        
        constexpr info_type obj_type() const noexcept { return info_type::sys_info; }

        constexpr const std::optional<std::string>&    manufacturer() const noexcept { return manufacturer_; }
        constexpr const std::optional<std::string>&    product_name() const noexcept { return product_name_; }
        constexpr const std::optional<std::string>&    sn() const noexcept { return sn_; }
        constexpr const std::optional<std::string>&    uuid() const noexcept { return uuid_; }

        double evaluate(const sysinfo& _other, const sysinfo_eval_conds& _conds) const {
            OBF_BEGIN;
            
            double result = 0.0;

            OBF_IF (is_equal(manufacturer_, _other.manufacturer_)) {
                result += _conds.manufacturer_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(product_name_, _other.product_name_)) {
                result += _conds.product_name_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(sn_, _other.sn_)) {
                result += _conds.sn_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(uuid_, _other.uuid_)) {
                result += _conds.uuid_relative_weight();
            } OBF_ENDIF;

            result /= _conds.member_count();
            OBF_RETURN(result);
            OBF_END
        }

        std::optional<std::string> manufacturer_;
        std::optional<std::string> product_name_;
        std::optional<std::string> sn_;
        std::optional<std::string> uuid_;
    };
    using sysinfo_ptr = std::shared_ptr<sysinfo>;

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_SYSINFO_HPP_INCLUDED
