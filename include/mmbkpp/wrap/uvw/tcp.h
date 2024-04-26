
#ifndef MMBKPP_WRAP_UVW_TCP_H_INCLUDED
#define MMBKPP_WRAP_UVW_TCP_H_INCLUDED

#include <uvw/tcp.h>
#include "stream.h"

#ifndef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace uvw {
    using TcpHandle = tcp_handle;
    
}
#endif

#endif // !MMBKPP_WRAP_UVW_TCP_H_INCLUDED
