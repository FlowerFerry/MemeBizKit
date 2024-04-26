
#ifndef MMBKPP_WRAP_UVW_STREAM_H_INCLUDED
#define MMBKPP_WRAP_UVW_STREAM_H_INCLUDED

#include <uvw/stream.h>
#include "handle.h"

#ifndef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace uvw {
    
    using ConnectEvent = connect_event;
    using DataEvent = data_event;
    using EndEvent = end_event;
    using WriteEvent = write_event;
    using ShutdownEvent = shutdown_event;
    using ListenEvent = listen_event;
}
#endif

#endif // !MMBKPP_WRAP_UVW_STREAM_H_INCLUDED
