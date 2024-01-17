
#ifndef MMBKPP_OBFHW_CPUINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_CPUINFO_HPP_INCLUDED

#include <stdint.h>

#include <string>
#include <memory>
#include <optional>

#include "basicinfo.hpp"

namespace mmbkpp {
namespace obfhw {
    
    struct cpuinfo 
    {
        cpuinfo()
        {}
        
        constexpr info_type obj_type() const noexcept { return info_type::processor_info; }

        const std::optional<std::string>&    sn() const noexcept { return sn_; }
        const std::optional<std::string>&    manufacturer() const noexcept { return manufacturer_; }
        const std::optional<uint8_t>&        type() const noexcept { return type_; }
        const std::optional<uint64_t>&       id() const noexcept { return id_; }
        const std::optional<int>&            core_count() const noexcept { return core_count_; }
        const std::optional<int>&            per_core_thread_count() const noexcept { return per_core_thread_count_; }
        const std::optional<int64_t>&        max_speed() const noexcept { return max_speed_; }
        

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
