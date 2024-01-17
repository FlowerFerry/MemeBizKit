
#ifndef MMBKPP_OBFHW_STRGINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_STRGINFO_HPP_INCLUDED

#include <stdint.h>

#include <string>
#include <memory>
#include <optional>

#include "basicinfo.hpp"

namespace mmbkpp {
namespace obfhw {
    
    struct strginfo 
    {
        strginfo() : 
            total_size_(0)
        {}

        constexpr info_type obj_type() const noexcept { return info_type::strg_info; }

        const std::optional<std::string>&    sn() const noexcept { return sn_; }
        const std::optional<int64_t>&        total_size() const noexcept { return total_size_; }

        std::optional<std::string> sn_;
        std::optional<int64_t>     total_size_;
    };
    using strginfo_ptr = std::shared_ptr<strginfo>;

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_STRGINFO_HPP_INCLUDED
