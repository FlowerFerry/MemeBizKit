
#ifndef MMBKPP_APP_OBJECT_FACTORY_HPP_INCLUDED
#define MMBKPP_APP_OBJECT_FACTORY_HPP_INCLUDED

#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>

namespace mmbkpp {
namespace app {

template <typename _Base>
class object_factory 
{
    object_factory() {}
public:
    typedef _Base base_type;
    typedef base_type* (creator_fn_t)(void* _user_data);
    typedef void (destroyer_fn_t)(base_type* _object, void* _user_data);

    inline void regi(
        const std::string& _key, creator_fn_t* _creator, destroyer_fn_t* _destroyer, void* _user_data = nullptr)
    {
        if (!_creator || !_destroyer)
            return;

        std::lock_guard<std::mutex> locker(mutex_);
        if (creators_.count(_key))
            return;
        
        creators_.insert(std::make_pair(_key, creator_info{ _creator, _destroyer, _user_data }));
    }

    inline void unregi(const std::string& _key)
    {
        std::lock_guard<std::mutex> locker(mutex_);
        creators_.erase(_key);
    }

    inline std::shared_ptr<base_type> create(const std::string& _key)
    {
        std::lock_guard<std::mutex> locker(mutex_);
        auto it = creators_.find(_key);
        if (it == creators_.end())
            return nullptr;

        auto p = it->second.creator(it->second.user_data);
        return std::shared_ptr<base_type>(p, 
        [destroyer = it->second.destroyer, user_data = it->second.user_data](void* _object) 
        {
            destroyer(reinterpret_cast<base_type*>(_object), user_data);
        });
    }

private:
    struct creator_info
    {
        creator_fn_t* creator;
        destroyer_fn_t* destroyer;
        void* user_data;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, creator_info> creators_;
};

}
}

#endif // !MMBKPP_APP_OBJECT_FACTORY_HPP_INCLUDED
