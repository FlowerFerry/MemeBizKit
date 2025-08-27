
#ifndef MMBKPP_CONTAINER_NESTED_HMAP_HPP_INCLUDED
#define MMBKPP_CONTAINER_NESTED_HMAP_HPP_INCLUDED

#include <unordered_map>
#include <type_traits>

/**
 * @file nested_hmap.hpp
 * @brief 该头文件实现了嵌套的哈希映射（unordered_map）结构，支持多层键值嵌套。
 *
 * 主要类：nested_layered_hmap，用于创建任意层数的嵌套无序映射。
 * 用途：简化多级键值数据的存储和访问，例如配置管理、数据分组等场景。
 * 作者/来源：基于提供的代码。
 * 注意：该实现使用模板递归，支持变参模板定义层数。
 * 示例：nested_hmap<int, std::string, int> 等价于 std::unordered_map<std::string, std::unordered_map<int, int>>。
 */

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

/**
 * @brief 层级配置结构体：nested_hmap_layer
 *
 * 用途：定义每一层的键类型、哈希函数和相等比较器。
 *
 * @tparam _TKey 该层的键类型。
 * @tparam _THash 哈希函数类型（默认为 std::hash<_TKey>）。
 * @tparam _TEqual 相等比较器类型（默认为 std::equal_to<_TKey>）。
 */
template<typename _TKey, typename _THash = std::hash<_TKey>, typename _TEqual = std::equal_to<_TKey>>
struct nested_hmap_layer
{
    using key_type = _TKey;
    using hash_type = _THash;
    using equal_type = _TEqual;
};

/**
 * @brief 主类：nested_layered_hmap（多层版本）
 *
 * 用途：实现多层嵌套的哈希映射。当前层是一个 unordered_map，映射到下一个层的 nested_layered_hmap。
 * 该类通过递归模板实现嵌套：如果有后续层，则 mapped_type 为下一个 nested_layered_hmap；否则为 _TValue。
 * 注意：该版本适用于两层或更多层；单层有特化版本。
 *
 * @tparam _THashMap 底层哈希映射的模板（例如 std::unordered_map，必须接受 <Key, Value, Hash, Equal> 参数）。
 * @tparam _TValue 最底层的值类型（leaf node 的类型）。
 * @tparam _TLayer 当前层的配置（nested_hmap_layer 类型）。
 * @tparam _TLayers 后续层的配置（变参，支持任意层数）。
 */
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

/**
 * @brief 主类：nested_layered_hmap（单层特化版本）
 *
 * 用途：当只剩一层时（基础递归情况），直接映射到 _TValue，而不是下一个映射。
 * 该特化处理嵌套的终止条件：当前层是 unordered_map<key_type, _TValue>。
 *
 * @tparam _THashMap 底层哈希映射的模板。
 * @tparam _TValue 值类型。
 * @tparam _TLayer 当前层的配置。
 */
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

/**
 * @brief 别名：nested_hmap
 *
 * 用途：简化创建嵌套映射的语法，使用 std::unordered_map 作为默认 _THashMap。
 *
 * @tparam _TValue 值类型。
 * @tparam _TKeys 各层的键类型（自动包装为 nested_hmap_layer<_TKeys>）。
 *
 * 示例：nested_hmap<double, std::string, int> 创建两层映射：string -> int -> double。
 */
template<typename _TValue, typename... _TKeys>
using nested_hmap = nested_layered_hmap<std::unordered_map, _TValue, nested_hmap_layer<_TKeys>...>;

}
}

#endif // !MMBKPP_CONTAINER_NESTED_HMAP_HPP_INCLUDED
