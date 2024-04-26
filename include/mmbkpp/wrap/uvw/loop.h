
#ifndef MMBKPP_WRAP_UVW_LOOP_H_INCLUDED
#define MMBKPP_WRAP_UVW_LOOP_H_INCLUDED

#include <uvw/loop.h>

#ifndef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace uvw {
    using Loop = loop;
    using ErrorEvent = error_event;
}
#endif

#endif // !MMBKPP_WRAP_UVW_LOOP_H_INCLUDED
