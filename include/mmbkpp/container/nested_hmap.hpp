
#ifndef MMBKPP_CONTAINER_NESTED_HMAP_HPP_INCLUDED
#define MMBKPP_CONTAINER_NESTED_HMAP_HPP_INCLUDED

#include <unordered_map>
#include <type_traits>

namespace mmbkpp {
namespace container {
namespace nested_hmap_details {

template<typename _TValue, typename _TLayer, typename... _TLayers>
struct function_inferred
{
    using value_type = typename function_inferred<_TValue, _TLayers...>::value_type;
};

template<typename _TValue, typename _TLayer>
struct function_inferred<_TValue, _TLayer>
{
    using value_type = _TValue;
};

}

template<typename _TKey, typename _THash = std::hash<_TKey>, typename _TEqual = std::equal_to<_TKey>>
struct nested_hmap_layer
{
    using key_type = _TKey;
    using hash_type = _THash;
    using equal_type = _TEqual;
};

template<template<typename...> typename _THashMap, typename _TValue, typename _TLayer, typename... _TLayers>
class nested_layered_hmap
{
public:
    using key_type = typename _TLayer::key_type;
    using mapped_type = _TValue;
    using hasher = typename _TLayer::hash_type;
    using key_equal = typename _TLayer::equal_type;
    using next_map_type = nested_layered_hmap<_THashMap, _TValue, _TLayers...>;
    using used_map_type = _THashMap<key_type, next_map_type, hasher, key_equal>;
    
    using iterator = typename used_map_type::iterator;
    using const_iterator = typename used_map_type::const_iterator;
    
    inline const next_map_type& operator[](const key_type& _key) const
    {
        auto it = maps_.find(_key);
        if (it == maps_.end())
            throw std::out_of_range("Key not found");
        return it->second;
    }

    inline next_map_type& operator[](const key_type& _key)
    {
        return maps_[_key];
    }

    inline bool empty() const noexcept
    {
        return maps_.empty();
    }

    inline size_t size() const noexcept
    {
        return maps_.size();
    }

    inline const_iterator find(const key_type& _key) const
    {
        return maps_.find(_key);
    }

    inline iterator find(const key_type& _key)
    {
        return maps_.find(_key);
    }

    inline const_iterator end() const
    {
        return maps_.end();
    }

    inline iterator end()
    {
        return maps_.end();
    }

    inline const_iterator begin() const
    {
        return maps_.begin();
    }
    
    inline iterator begin()
    {
        return maps_.begin();
    }

    inline const used_map_type& maps() const noexcept
    {
        return maps_;
    }

    inline used_map_type& maps() noexcept
    {
        return maps_;
    }

    template<typename ... _TArgs>
    inline const typename nested_hmap_details::function_inferred<_TValue, _TLayers...>::value_type* get(const key_type& _key, _TArgs&&... _args) const noexcept
    {
        auto it = maps_.find(_key);
        if (it == maps_.end())
            return nullptr;
        return it->second.get(std::forward<_TArgs>(_args)...);
    }

    template<typename ... _TArgs>
    inline typename nested_hmap_details::function_inferred<_TValue, _TLayers...>::value_type* get(const key_type& _key, _TArgs&&... _args) noexcept
    {
        auto it = maps_.find(_key);
        if (it == maps_.end())
            return nullptr;
        return it->second.get(std::forward<_TArgs>(_args)...);
    }

    inline const auto* get() const noexcept
    {
        return this;
    }

    inline auto* get() noexcept
    {
        return this;
    }

    template<typename... _TArgs>
    inline auto value_or(
        const typename nested_hmap_details::function_inferred<_TValue, _TLayers...>::value_type& _default,
        const key_type& _key, 
        _TArgs&&... _args) const noexcept
    {
        auto it = maps_.find(_key);
        if (it == maps_.end())
            return _default;
        return it->second.value_or(_default, std::forward<_TArgs>(_args)...);
    }

    inline void clear() noexcept
    {
        maps_.clear();
    }
    
private:
    used_map_type maps_;
};

template<template<typename...> typename _THashMap, typename _TValue, typename _TLayer>
class nested_layered_hmap<_THashMap, _TValue, _TLayer>
{
public:
    using key_type = typename _TLayer::key_type;
    using mapped_type = _TValue;
    using hasher = typename _TLayer::hash_type;
    using key_equal = typename _TLayer::equal_type;
    using used_map_type = _THashMap<key_type, mapped_type, hasher, key_equal>;
    
    using iterator = typename used_map_type::iterator;
    using const_iterator = typename used_map_type::const_iterator;

    inline const mapped_type& operator[](const key_type& _key) const
    {
        auto it = maps_.find(_key);
        if (it == maps_.end())
            throw std::out_of_range("Key not found");
        return it->second;
    }

    inline mapped_type& operator[](const key_type& _key)
    {
        return maps_[_key];
    }

    inline bool empty() const noexcept
    {
        return maps_.empty();
    }

    inline size_t size() const noexcept
    {
        return maps_.size();
    }

    inline const_iterator find(const key_type& _key) const
    {
        return maps_.find(_key);
    }

    inline iterator find(const key_type& _key)
    {
        return maps_.find(_key);
    }

    inline const_iterator end() const
    {
        return maps_.end();
    }

    inline iterator end()
    {
        return maps_.end();
    }

    inline const_iterator begin() const
    {
        return maps_.begin();
    }

    inline iterator begin()
    {
        return maps_.begin();
    }

    inline const used_map_type& maps() const noexcept
    {
        return maps_;
    }

    inline used_map_type& maps() noexcept
    {
        return maps_;
    }

    template<typename ... _TArgs>
    inline const typename nested_hmap_details::function_inferred<_TValue, _TLayer>::value_type* get(const key_type& _key, _TArgs&&... _args) const noexcept
    {
        auto it = maps_.find(_key);
        if (it == maps_.end())
            return nullptr;
        return &it->second;
    }

    template<typename ... _TArgs>
    inline typename nested_hmap_details::function_inferred<_TValue, _TLayer>::value_type* get(const key_type& _key, _TArgs&&... _args) noexcept
    {
        auto it = maps_.find(_key);
        if (it == maps_.end())
            return nullptr;
        return &it->second;
    }

    inline const auto* get() const noexcept
    {
        return this;
    }

    inline auto* get() noexcept
    {
        return this;
    }

    template<typename... _TArgs>
    inline mapped_type value_or(
        const typename nested_hmap_details::function_inferred<_TValue, _TLayer>::value_type& _default,
        const key_type& _key) const noexcept
    {
        auto it = maps_.find(_key);
        if (it == maps_.end())
            return _default;
        return it->second;
    }    

    inline void clear() noexcept
    {
        maps_.clear();
    }

private:
    used_map_type maps_;
};

template<typename _TValue, typename... _TKeys>
using nested_hmap = nested_layered_hmap<std::unordered_map, _TValue, nested_hmap_layer<_TKeys>...>;

}
}

#endif // !MMBKPP_CONTAINER_NESTED_HMAP_HPP_INCLUDED
