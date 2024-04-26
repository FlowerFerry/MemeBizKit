
#ifndef MMBKPP_WRAP_UVW_TCP_H_INCLUDED
#define MMBKPP_WRAP_UVW_TCP_H_INCLUDED

#include <uvw/tcp.h>
#include "stream.h"

#ifndef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
namespace uvw {
    using TCPHandle = tcp_handle;
    
}
#endif

namespace mmbkpp {
    inline int uvw_bind_ipv4only(uvw::TCPHandle& _hdl, const std::string& _host, unsigned _port)
    {
#ifndef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED        
        sockaddr_storage storage;
        memset(&storage, 0, sizeof(storage));
        if (uv_ip4_addr(_host.data(), _port, (sockaddr_in*)&storage) == 0)
        {
        }
        else if (uv_ip6_addr(_host.data(), _port, (sockaddr_in6*)&storage) == 0)
        {
        }
        return _hdl.bind(*((sockaddr*)&storage));
#else
        return _hdl.bind<uvw::IPv4>(_host, _port);
#endif
    }

    inline int uvw_bind(uvw::TCPHandle& _hdl, const std::string& _host, unsigned _port)
    {
#ifndef MMBKPP_WRAP_UVW_3_0_AFTER_DISABLED
        sockaddr_storage storage;
        memset(&storage, 0, sizeof(storage));
        if (uv_ip4_addr(_host.data(), _port, (sockaddr_in*)&storage) == 0) 
        {
        }
        else if (uv_ip6_addr(_host.data(), _port, (sockaddr_in6*)&storage) == 0) 
        {
        }
        return _hdl.bind(*((sockaddr*)&storage));
#else
        return _hdl.bind<uvw::IPv6>(_host, _port);
#endif
    }
}

#endif // !MMBKPP_WRAP_UVW_TCP_H_INCLUDED
