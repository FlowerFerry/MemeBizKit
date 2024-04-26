
#ifndef MMBKPP_WRAP_UVW_TIMER_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_TIMER_FWD_H_INCLUDED

#include "emitter_fwd.h"

namespace uvw {
#ifdef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
    class TimerHandle;
    struct TimerEvent;
#else
    class timer_handle;
    struct timer_event;
    using TimerHandle = timer_handle;
    using TimerEvent = timer_event;
#endif
};


#endif // !MMBKPP_WRAP_UVW_TIMER_FWD_H_INCLUDED
