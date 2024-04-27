
#ifndef MMBKPP_WRAP_UVW_LOOP_H_INCLUDED
#define MMBKPP_WRAP_UVW_LOOP_H_INCLUDED

#include <uvw/loop.h>
#include "emitter.h"

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    using Loop = loop;
}
#endif

namespace mmbkpp {
    
    enum class uvw_run_mode 
    {
        DEFAULT = 0,
        ONCE,
        NOWAIT
    };

    inline int uvw_loop_run(uvw::Loop& _loop, uvw_run_mode _mode = uvw_run_mode::DEFAULT) noexcept
    {
#ifdef MMBKPP_WRAP_UVW_3_0_DISABLED
        switch (_mode) {
        case uvw_run_mode::DEFAULT:
            return _loop.run();
        case uvw_run_mode::ONCE:
            return _loop.run<uvw::Loop::Mode::ONCE>();
        case uvw_run_mode::NOWAIT:
            return _loop.run<uvw::Loop::Mode::NOWAIT>();
        default:
            return -1;
        }
#else
        return _loop.run(static_cast<uvw::loop::run_mode>(_mode));
#endif
    }
}

#endif // !MMBKPP_WRAP_UVW_LOOP_H_INCLUDED
