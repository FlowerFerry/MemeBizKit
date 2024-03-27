
#ifndef MMUPP_APP_TXTFILE_HANDLER_HPP_INCLUDED
#define MMUPP_APP_TXTFILE_HANDLER_HPP_INCLUDED

namespace mmbkpp::app {
    
    // template<typename _Object>
    // class txtfile_handler
    // {
        //using object_t = _Object;
        //using object_ptr_t = std::shared_ptr<object_t>;

        //inline object_ptr_t default_object()
        //{
        //    return std::make_shared<object_t>();
        //}

        //inline outcome::checked<object_ptr_t, mgpp::err> deserialize(const std::string& _str)
        //{
        //    return outcome::failure(mgpp::err{ MGEC__ERR });
        //}

        //inline outcome::checked<std::string, mgpp::err> serialize(const object_t& _obj)
        //{
        //    return outcome::failure(mgpp::err{ MGEC__ERR });
        //}
    // };

    template<typename _Object>
    struct serializer;
    // {
    //     static inline outcome::checked<std::string, mgpp::err> serialize(const _Object& _obj)
    //     {
    //         return outcome::failure(mgpp::err{ MGEC__ERR });
    //     }
    // }

    template<typename _Object>
    struct deserializer;
    // {
    //     static inline outcome::checked<std::shared_ptr<_Object>, mgpp::err> deserialize(const std::string& _str)
    //     {
    //         return outcome::failure(mgpp::err{ MGEC__ERR });
    //     }
    // }

    template<typename _Object>
    struct default_creator;
    // {
    //     static inline std::shared_ptr<_Object> create()
    //     {
    //         return std::make_shared<_Object>();
    //     }
    // }
}

#endif // !MMUPP_APP_TXTFILE_HANDLER_HPP_INCLUDED
