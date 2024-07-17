
#ifndef MMBKPP_WRAP_UVW_TCP_H_INCLUDED
#define MMBKPP_WRAP_UVW_TCP_H_INCLUDED

#include <uvw/tcp.h>
#include "stream.h"

#include <string.h>

#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
namespace uvw {
    using TCPHandle = tcp_handle;
    
}
#endif

namespace mmbkpp {
    inline bool uvw_set_keep_alive(uvw::TCPHandle& _hdl, bool _enable, std::chrono::duration<unsigned int> _val)
    {
#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED 
        return _hdl.keep_alive(_enable, _val);
#else
        return _hdl.keepAlive(_enable, _val);
#endif
    }

    inline int uvw_bind_ipv4only(uvw::TCPHandle& _hdl, const std::string& _host, unsigned _port)
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
        return _hdl.bind(*((sockaddr*)&storage));
#else
        return _hdl.bind<uvw::IPv4>(_host, _port);
#endif
    }

    inline int uvw_bind(uvw::TCPHandle& _hdl, const std::string& _host, unsigned _port)
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
        return _hdl.bind(*((sockaddr*)&storage));
#else
        return _hdl.bind<uvw::IPv6>(_host, _port);
#endif
    }


#ifndef MMBKPP_WRAP_UVW_3_0_DISABLED
    inline uvw::socket_address uvw_getpeer(const uvw::TCPHandle& _hdl)
    {
        return _hdl.peer();
    }
    inline uvw::socket_address uvw_getsock(const uvw::TCPHandle& _hdl)
    {
        return _hdl.sock();
    }
    inline uvw::socket_address uvw_getpeer_ipv4try(const uvw::TCPHandle& _hdl)
    {
        return _hdl.peer();
    }
    inline uvw::socket_address uvw_getsock_ipv4try(const uvw::TCPHandle& _hdl)
    {
        return _hdl.sock();
    }
#else
    inline uvw::Addr uvw_getpeer(const uvw::TCPHandle& _hdl)
    {
        return _hdl.peer();
    }
    inline uvw::Addr uvw_getsock(const uvw::TCPHandle& _hdl)
    {
        return _hdl.sock();
    }
    inline uvw::Addr uvw_getpeer_ipv4try(const uvw::TCPHandle& _hdl)
    {
        return _hdl.peer<uvw::IPv4>();
    }
    inline uvw::Addr uvw_getsock_ipv4try(const uvw::TCPHandle& _hdl)
    {
        return _hdl.sock<uvw::IPv4>();
    }
#endif

}

#endif // !MMBKPP_WRAP_UVW_TCP_H_INCLUDED
