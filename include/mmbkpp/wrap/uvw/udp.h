
#ifndef MMBKPP_WRAP_UVW_UDP_H_INCLUDED
#define MMBKPP_WRAP_UVW_UDP_H_INCLUDED

#include <uvw/udp.h>
#include "stream.h"

#include <string.h>

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    using UDPHandle = udp_handle;
    using UDPFlags  = udp_handle::udp_flags;
}
#else
namespace uvw {
    using UDPFlags  = uvw::UDPHandle::Bind;
}
#endif

namespace mmbkpp {

    inline int uvw_bind_ipv4only(
        uvw::UDPHandle& _hdl, const std::string& _host, unsigned _port, 
        uvw::UDPFlags _flags = uvw::UDPFlags::_UVW_ENUM)
    {
#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
        sockaddr_storage storage;
        memset(&storage, 0, sizeof(storage));
        if (uv_ip4_addr(_host.data(), _port, (sockaddr_in*)&storage) == 0)
        {
        }
        else if (uv_ip6_addr(_host.data(), _port, (sockaddr_in6*)&storage) == 0)
        {
        }
        return _hdl.bind(*((sockaddr*)&storage), _flags);
#else
        return _hdl.bind<uvw::IPv4>(_host, _port, _flags);
#endif
    }

}

#endif // !MMBKPP_WRAP_UVW_UDP_H_INCLUDED
