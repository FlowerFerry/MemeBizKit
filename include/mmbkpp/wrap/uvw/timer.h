
#ifndef MMBKPP_WRAP_UVW_TIMER_H_INCLUDED
#define MMBKPP_WRAP_UVW_TIMER_H_INCLUDED

#include <uvw/timer.h>
#include "handle.h"

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    using TimerHandle = timer_handle;
    using TimerEvent  = timer_event;
}
#endif

#endif // !MMBKPP_WRAP_UVW_TIMER_H_INCLUDED
