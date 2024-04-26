
#ifndef MMBKPP_WRAP_UVW_EMITTER_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_EMITTER_FWD_H_INCLUDED

#ifndef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace uvw {
    struct error_event;
    using ErrorEvent = error_event;
}
#else
namespace uvw {
    struct ErrorEvent;
}
#endif

#endif // !MMBKPP_WRAP_UVW_EMITTER_FWD_H_INCLUDED
