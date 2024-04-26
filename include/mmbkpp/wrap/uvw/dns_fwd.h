
#ifndef MMBKPP_WRAP_UVW_DNS_FWD_H_INCLUDED
#define MMBKPP_WRAP_UVW_DNS_FWD_H_INCLUDED

#ifdef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace uvw {
    class GetAddrInfoReq;
    struct AddrInfoEvent;
    
    class GetNameInfoReq;
    struct NameInfoEvent;
}
#else
namespace uvw {
    class get_addr_info_req;
    struct addr_info_event;

    class get_name_info_req;
    struct name_info_event;

    using GetAddrInfoReq = get_addr_info_req;
    using AddrInfoEvent  = addr_info_event;

    using GetNameInfoReq = get_name_info_req;
    using NameInfoEvent  = name_info_event;
}
#endif

#endif // !MMBKPP_WRAP_UVW_DNS_FWD_H_INCLUDED
