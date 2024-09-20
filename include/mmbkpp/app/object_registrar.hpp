
#ifndef MMBKPP_APP_OBJECT_REGISTRAR_HPP_INCLUDED
#define MMBKPP_APP_OBJECT_REGISTRAR_HPP_INCLUDED

#include "object_factory.hpp"

namespace mmbkpp {
namespace app {

template <typename _Base, typename _Derived>
class object_registrar
{
public:
    object_registrar(const std::string& _key)
    {
        object_factory<_Base>::instance().regi(_key, 
        [](void* _user_data) -> _Base* { return new _Derived(); },
        [](base_type* _object, void* _user_data) { delete static_cast<_Derived*>(_object); }, 
        nullptr);
    }

};

}
}

#endif // !MMBKPP_APP_OBJECT_REGISTRAR_HPP_INCLUDED
