
#ifndef MMBKPP_OBFHW_MEMINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_MEMINFO_HPP_INCLUDED

#include <stdint.h>

#include <string>
#include <memory>
#include <optional>

#include "basicinfo.hpp"

namespace mmbkpp {
namespace obfhw {
    
    struct meminfo 
    {
        meminfo()
        {}
        
        constexpr info_type obj_type() const noexcept { return info_type::mem_info; }

        const std::optional<int>&            type() const noexcept { return type_; }
        const std::optional<int64_t>&        total_size() const noexcept { return total_size_; }
        const std::optional<int64_t>&        max_speed() const noexcept { return max_speed_; }

        std::optional<int>     type_;
        std::optional<int64_t> total_size_;
        std::optional<int64_t> max_speed_;
    };
    using meminfo_ptr = std::shared_ptr<meminfo>;

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_MEMINFO_HPP_INCLUDED
