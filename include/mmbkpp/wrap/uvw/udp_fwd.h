
#ifndef MMBKPP_WRAP_UVW_UDP_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_UDP_FWD_H_INCLUDED

namespace uvw {
#ifdef MMBKPP_WRAP_UVW_3_0_DISABLED
    class UDPHandle;
#else
    class udp_handle;
    using UDPHandle = udp_handle;
#endif
};

#endif // !MMBKPP_WRAP_UVW_UDP_FWD_H_INCLUDED
