
#ifndef MMBKPP_WRAP_UVW_UDP_H_INCLUDED
#define MMBKPP_WRAP_UVW_UDP_H_INCLUDED

#include <uvw/udp.h>
#include "stream.h"

#include <string.h>

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    using UDPHandle = udp_handle;
    
}
#endif

#endif // !MMBKPP_WRAP_UVW_UDP_H_INCLUDED
