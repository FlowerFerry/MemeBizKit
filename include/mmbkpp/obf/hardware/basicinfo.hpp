
#ifndef MMBKPP_OBFHW_BASICINFO_HPP_INCLUDED
#define MMBKPP_OBFHW_BASICINFO_HPP_INCLUDED

#include <stdint.h>
#include <variant>
#include <string>
#include <optional>

namespace mmbkpp {
namespace obfhw  {
    
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

    struct basic_eval_conds {
        virtual ~basic_eval_conds() = default;
    };

    template<typename T>
    inline bool is_equal(const std::optional<T>& _a, const std::optional<T>& _b) 
    {
        if (_a && _b) {
            return _a.value() == _b.value();
        }
        if (!_a && !_b) {
            return true;
        }
        return false;
    }

}
} // namespace mmbkpp

#endif // !MMBKPP_OBFHW_BASICINFO_HPP_INCLUDED
