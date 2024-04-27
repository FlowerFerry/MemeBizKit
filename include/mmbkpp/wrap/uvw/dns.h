
#ifndef MMBKPP_WRAP_UVW_DNS_H_INCLUDED
#define MMBKPP_WRAP_UVW_DNS_H_INCLUDED

#include <uvw/dns.h>

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    using GetAddrInfoReq = get_addr_info_req;
    using AddrInfoEvent  = addr_info_event;

    using GetNameInfoReq = get_name_info_req;
    using NameInfoEvent  = name_info_event;
}
#endif

#endif // !MMBKPP_WRAP_UVW_DNS_H_INCLUDED
