
#ifndef MMBKPP_OBFHW_SYSINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_SYSINFO_HPP_INCLUDED

#include <stdint.h>

#include <string>
#include <memory>
#include <optional>

#include "basicinfo.hpp"

namespace mmbkpp {
namespace obfhw {
    
    struct sysinfo 
    {
        sysinfo()
        {}
        
        constexpr info_type obj_type() const noexcept { return info_type::sys_info; }

        const std::optional<std::string>&    manufacturer() const noexcept { return manufacturer_; }
        const std::optional<std::string>&    product_name() const noexcept { return product_name_; }
        const std::optional<std::string>&    sn() const noexcept { return sn_; }
        const std::optional<std::string>&    uuid() const noexcept { return uuid_; }


        std::optional<std::string> manufacturer_;
        std::optional<std::string> product_name_;
        std::optional<std::string> sn_;
        std::optional<std::string> uuid_;
    };
    using sysinfo_ptr = std::shared_ptr<sysinfo>;

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_SYSINFO_HPP_INCLUDED
