
#ifndef MMBKPP_WRAP_UVW_TCP_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_TCP_FWD_H_INCLUDED

namespace uvw {
#ifdef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
    class TCPHandle;
#else
    class tcp_handle;
    using TCPHandle = tcp_handle;
#endif
};

#endif // !MMBKPP_WRAP_UVW_TCP_FWD_H_INCLUDED
