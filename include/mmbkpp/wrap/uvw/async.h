
#ifndef MMBKPP_WRAP_UVW_ASYNC_H_INCLUDED
#define MMBKPP_WRAP_UVW_ASYNC_H_INCLUDED

#include <uvw/async.h>
#include "handle.h"

#ifndef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace uvw {
    using AsyncHandle = async_handle;
    using AsyncEvent  = async_event;
}
#endif

#endif // !MMBKPP_WRAP_UVW_ASYNC_H_INCLUDED
