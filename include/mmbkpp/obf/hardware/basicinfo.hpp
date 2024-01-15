
#ifndef MMBKPP_OBFHW_BASICINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_BASICINFO_HPP_INCLUDED

#include <stdint.h>
#include <variant>
#include <string>

namespace mmbkpp {
namespace obfhw {
    
    enum class info_type : uint8_t 
    {
        sys_info = 1,
        processor_info = 4,

        mem_info  = 0xFD,
        strg_info = 0xFC,
        none = 0xFF
    };

    using variant = std::variant<
        std::monostate,
        std::string,
        uint64_t,
        int64_t
    >;

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_BASICINFO_HPP_INCLUDED
