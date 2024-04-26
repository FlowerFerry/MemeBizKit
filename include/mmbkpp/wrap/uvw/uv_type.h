
#ifndef MMBKPP_WRAP_UVW_UV_TYPE_H_INCLUDED
#define MMBKPP_WRAP_UVW_UV_TYPE_H_INCLUDED

#include <uvw/loop.h>

#ifdef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace mmbkpp {
    template<typename _Ty>
    inline uvw::loop& get_uvw_loop(const uvw::uv_type<_Ty>& _handle) noexcept
    {
        return _handle.loop();
    }
}
#else
namespace mmbkpp {
    template<typename _Ty>
    inline uvw::loop& get_uvw_loop(const uvw::uv_type<_Ty>& _handle) noexcept
    {
        return _handle.parent();
    }
}
#endif

#endif // !MMBKPP_WRAP_UVW_UV_TYPE_H_INCLUDED
