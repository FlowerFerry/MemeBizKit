
#ifndef MMBKPP_WRAP_UVW_STREAM_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_STREAM_FWD_H_INCLUDED

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    
    struct ConnectEvent;
    struct DataEvent;
    struct EndEvent;
    struct WriteEvent;
    struct ShutdownEvent;
    struct ListenEvent;
}
#else
namespace uvw {
    
    struct connect_event;
    struct data_event;
    struct end_event;
    struct write_event;
    struct shutdown_event;
    struct listen_event;

    using ConnectEvent = connect_event;
    using DataEvent = data_event;
    using EndEvent = end_event;
    using WriteEvent = write_event;
    using ShutdownEvent = shutdown_event;
    using ListenEvent = listen_event;
    
}
#endif

#endif // !MMBKPP_WRAP_UVW_STREAM_FWD_H_INCLUDED
