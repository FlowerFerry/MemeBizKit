
#ifndef MMBKPP_OBFHW_CPUINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_CPUINFO_HPP_INCLUDED

#include <stdint.h>

#include <string>
#include <memory>
#include <optional>

#include "basicinfo.hpp"
#include <mmbkpp/wrap/obfy/instr.h>

namespace mmbkpp {
namespace obfhw  {
    
    struct cpuinfo_eval_conds : public basic_eval_conds
    {

        constexpr int member_count() const noexcept { return 7; }

        constexpr double total_weight() const noexcept 
        { 
            return sn_weight + manufacturer_weight + type_weight + id_weight + core_count_weight + per_core_thread_count_weight + max_speed_weight;
        }

        constexpr double sn_relative_weight() const noexcept 
        { 
            return sn_weight / total_weight() * member_count();
        }

        constexpr double manufacturer_relative_weight() const noexcept 
        { 
            return manufacturer_weight / total_weight() * member_count();
        }

        constexpr double type_relative_weight() const noexcept 
        { 
            return type_weight / total_weight() * member_count();
        }

        constexpr double id_relative_weight() const noexcept 
        { 
            return id_weight / total_weight() * member_count();
        }

        constexpr double core_count_relative_weight() const noexcept 
        { 
            return core_count_weight / total_weight() * member_count();
        }

        constexpr double per_core_thread_count_relative_weight() const noexcept 
        { 
            return per_core_thread_count_weight / total_weight() * member_count();
        }

        constexpr double max_speed_relative_weight() const noexcept 
        { 
            return max_speed_weight / total_weight() * member_count();
        }

        int8_t sn_weight = 100;
        int8_t manufacturer_weight = 80;
        int8_t type_weight = 80;
        int8_t id_weight = 100;
        int8_t core_count_weight = 80;
        int8_t per_core_thread_count_weight = 80;
        int8_t max_speed_weight = 80;
    };

    struct cpuinfo 
    {
        cpuinfo()
        {}
        
        constexpr info_type obj_type() const noexcept { return info_type::processor_info; }

        constexpr const std::optional<std::string>&    sn() const noexcept { return sn_; }
        constexpr const std::optional<std::string>&    manufacturer() const noexcept { return manufacturer_; }
        constexpr const std::optional<uint8_t>&        type() const noexcept { return type_; }
        constexpr const std::optional<uint64_t>&       id() const noexcept { return id_; }
        constexpr const std::optional<int>&            core_count() const noexcept { return core_count_; }
        constexpr const std::optional<int>&            per_core_thread_count() const noexcept { return per_core_thread_count_; }
        constexpr const std::optional<int64_t>&        max_speed() const noexcept { return max_speed_; }

        double evaluate(const cpuinfo& other, const cpuinfo_eval_conds& conds) const 
        {
            OBF_BEGIN

            double score = 0.0;
            OBF_IF (is_equal(sn(), other.sn())) {
                score += conds.sn_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(manufacturer(), other.manufacturer())) 
            {
                score += conds.manufacturer_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(type(), other.type())) 
            {
                score += conds.type_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(id(), other.id())) 
            {
                score += conds.id_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(core_count(), other.core_count())) 
            {
                score += conds.core_count_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(per_core_thread_count(), other.per_core_thread_count())) 
            {
                score += conds.per_core_thread_count_relative_weight();
            } OBF_ENDIF;

            OBF_IF (is_equal(max_speed(), other.max_speed())) {
                score += conds.max_speed_relative_weight();
            } OBF_ENDIF;

            score /= conds.member_count();
            OBF_RETURN(score);
            OBF_END
        }        

        std::optional<std::string>    sn_;
        std::optional<std::string>    manufacturer_;
        std::optional<uint8_t>        type_;
        std::optional<uint64_t>       id_;
        std::optional<int>            core_count_;
        std::optional<int>            per_core_thread_count_;
        std::optional<int64_t>        max_speed_;
    };
    using cpuinfo_ptr = std::shared_ptr<cpuinfo>;

    using processor_info = cpuinfo;
    using processor_info_ptr = cpuinfo_ptr;

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_CPUINFO_HPP_INCLUDED
