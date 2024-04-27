
#ifndef MMBKPP_WRAP_UVW_EMITTER_H_INCLUDED
#define MMBKPP_WRAP_UVW_EMITTER_H_INCLUDED

#include <uvw/emitter.h>

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    using ErrorEvent = error_event;
}
#endif

namespace mmbkpp {
    template<typename _Event, typename _Emitter>
    inline void uvw_reset_event(_Emitter& _emitter) {
#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
        _emitter.template reset<_Event>();
#else
        _emitter.template clear<_Event>();
#endif
    }
}

#endif // !MMBKPP_WRAP_UVW_EMITTER_H_INCLUDED
