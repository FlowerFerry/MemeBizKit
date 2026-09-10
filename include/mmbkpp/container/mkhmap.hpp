
#ifndef MMBKPP_CONTAINER_MKHMAP_HPP_INCLUDED
#define MMBKPP_CONTAINER_MKHMAP_HPP_INCLUDED

#include <unordered_map>
#include <tuple>

namespace mmbkpp {
namespace container {

    template<size_t _Index, typename _Ty, typename... _Ts>
    struct mk_nth_helper
    {
        using type = typename mk_nth_helper<_Index - 1, _Ts...>::type;
    };

    template<typename _Ty, typename... _Ts>
    struct mk_nth_helper<0, _Ty, _Ts...>
    {
        using type = _Ty;
    };

    template<typename _TKey, typename _THash = std::hash<_TKey>, typename _TEqual = std::equal_to<_TKey>>
    struct mkhmap_layer
    {
        using key_type = _TKey;
        using hash_type = _THash;
        using equal_type = _TEqual;
    };

    template<typename _TLayer, typename... _TLayers>
    using mk_tuple = std::tuple<typename _TLayer::key_type, typename _TLayers::key_type...>;

    template<size_t _Index, typename _TLayer, typename... _TLayers>
    struct mk_wrap
    {
        mk_wrap() {}
        // TO_DO
        mk_wrap(const typename mk_nth_helper<_Index, _TLayer, _TLayers...>::type::key_type& _key)
        {
            std::get<_Index>(tuple_) = _key;
        }
        mk_wrap(const mk_tuple<_TLayer, _TLayers...>& _tuple)
            : tuple_(_tuple)
        {
        }
        mk_wrap(const mk_wrap&) = default;
        mk_wrap(mk_wrap&&) = default;
        
        inline bool operator==(const mk_wrap& _key) const noexcept
        {
            return std::get<_Index>(tuple_) == std::get<_Index>(_key.tuple_);
        }

        inline const typename mk_nth_helper<_Index, _TLayer, _TLayers...>::type::key_type& key() const noexcept
        {
            return std::get<_Index>(tuple_);
        }

        template<size_t _CurrIndex>
        inline const typename mk_nth_helper<_CurrIndex, _TLayer, _TLayers...>::type::key_type& key() const noexcept
        {
            return std::get<_CurrIndex>(tuple_);
        }

        inline const mk_tuple<_TLayer, _TLayers...>& tuple() const noexcept
        {
            return tuple_;
        }
        
        mk_tuple<_TLayer, _TLayers...> tuple_;
    };

    template<size_t _Index, typename... _TLayers>
    struct mk_hash
    {
        inline std::size_t operator()(const mk_wrap<_Index, _TLayers...>& _k) const
        {
            return typename mk_nth_helper<_Index, _TLayers...>::type::hash_type()(_k.key());
        }
    };

    template<size_t _Index, typename... _TLayers>
    struct mk_equal
    {
        inline bool operator()(const mk_wrap<_Index, _TLayers...>& _k1, const mk_wrap<_Index, _TLayers...>& _k2) const
        {
            return typename mk_nth_helper<_Index, _TLayers...>::type::equal_type()(_k1.key(), _k2.key());
        }
    };

    template<template<typename...> typename _THashMap, typename _TValue, size_t _Index, typename _TLayer, typename... _TLayers>
    struct mk_map_tuple
    {
        using map_type = _THashMap<
            mk_wrap<_Index, _TLayer, _TLayers...>, 
            _TValue, 
            mk_hash<_Index, _TLayer, _TLayers...>,
            mk_equal<_Index, _TLayer, _TLayers...>
        >;
        using type = decltype(std::tuple_cat(std::declval<typename mk_map_tuple<_THashMap, _TValue, _Index - 1, _TLayer, _TLayers...>::type>(), std::tuple<map_type>()));
    };

    template<template<typename...> typename _THashMap, typename _TValue, typename _TLayer, typename... _TLayers>
    struct mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>
    {
        using map_type = _THashMap<
            mk_wrap<0, _TLayer, _TLayers...>, 
            _TValue,
            mk_hash<0, _TLayer, _TLayers...>,
            mk_equal<0, _TLayer, _TLayers...>
        >;
        using type = std::tuple<map_type>;
    };

    namespace mk_details {

        template<size_t _Index>
        struct exist_eacher {
            template<typename _TMapTuple, typename... _TKeys>
            static bool exist(const _TMapTuple& _maps, const std::tuple<_TKeys...>& _key) {
                return std::get<_Index>(_maps).find(std::get<_Index>(_key)) != std::get<_Index>(_maps).end()
                    || exist_eacher<_Index - 1>::exist(_maps, _key);
            }
        };

        template<>
        struct exist_eacher<0> {
            template<typename _TMapTuple, typename... _TKeys>
            static bool exist(const _TMapTuple& _maps, const std::tuple<_TKeys...>& _key) {
                return std::get<0>(_maps).find(std::get<0>(_key)) != std::get<0>(_maps).end();
            }
        };

        template<size_t _IgnoreIndex, size_t _Index>
        struct eraser {
            template<typename _TMapTuple, typename _TKey, typename... _TKeys>
            static void erase(_TMapTuple& _maps, const std::tuple<_TKey, _TKeys...>& _key) {
                if constexpr (_Index != _IgnoreIndex) {
                    std::get<_Index>(_maps).erase(std::get<_Index>(_key));
                }
                eraser<_IgnoreIndex, _Index - 1>::erase(_maps, _key);
            }
        };

        template<size_t _IgnoreIndex>
        struct eraser<_IgnoreIndex, 0> {
            template<typename _TMapTuple, typename _TKey, typename... _TKeys>
            static void erase(_TMapTuple& _maps, const std::tuple<_TKey, _TKeys...>& _key) {
                if constexpr (0 != _IgnoreIndex) {
                    std::get<0>(_maps).erase(std::get<0>(_key));
                }
            }
        };

        template<size_t _Index>
        struct clearer {
            template<typename _TMapTuple>
            static void clear(_TMapTuple& _maps) noexcept
            {
                clearer<_Index - 1>::clear(_maps);
                std::get<_Index>(_maps).clear();
            }
        };

        template<>
        struct clearer<0> {
            template<typename _TMapTuple>
            static void clear(_TMapTuple& _maps) noexcept
            {
                std::get<0>(_maps).clear();
            }
        };

        template<size_t _Index>
        struct inserter {
            template<typename _TMapTuple, typename _TValue, typename _TKey, typename... _TKeys>
            static bool insert(_TMapTuple& _maps, const std::tuple<_TKey, _TKeys...>& _key, const _TValue& _value)
            {
                try {
                    if (std::get<_Index>(_maps).insert(std::make_pair(_key, _value)).second == false) {
                        eraser<_Index, sizeof...(_TKeys)>::erase(_maps, _key);
                        return false;
                    }

                    return inserter<_Index - 1>::insert(_maps, _key, _value);
                } catch (...) {
                    eraser<_Index, sizeof...(_TKeys)>::erase(_maps, _key);
                    throw;
                }
            }
        };

        template<>
        struct inserter<0> {
            template<typename _TMapTuple, typename _TValue, typename _TKey, typename... _TKeys>
            static bool insert(_TMapTuple& _maps, const std::tuple<_TKey, _TKeys...>& _key, const _TValue& _value)
            {
                try {
                    if (std::get<0>(_maps).insert(std::make_pair(_key, _value)).second == false) {
                        eraser<0, sizeof...(_TKeys)>::erase(_maps, _key);
                        return false;
                    }
                } catch (...) {
                    eraser<0, sizeof...(_TKeys)>::erase(_maps, _key);
                    throw;
                }
                return true;
            }
        };

    };
    
    //! 这个类实现了一个支持多个主键的哈希表，每个主键都能 O(1) 查找同一份数据，适合需要多重唯一索引的场景。
    template<template<typename...> typename _THashMap, typename _TValue, typename _TLayer, typename... _TLayers>
    struct mk_layered_hmap
    {
        using map_tuple_type = typename mk_map_tuple<_THashMap, _TValue, sizeof...(_TLayers), _TLayer, _TLayers...>::type;
        
        using iterator = typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::iterator;
        using const_iterator = typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::const_iterator;

        inline bool empty() const noexcept
        {
            return std::get<0>(maps_).empty();
        }

        inline size_t size() const noexcept
        {
            return std::get<0>(maps_).size();
        }

        template<size_t _Index>
        inline typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::iterator
            find(const typename mk_nth_helper<_Index, _TLayer, _TLayers...>::type::key_type& _key)
        {
            return std::get<_Index>(maps_).find(_key);
        }

        template<size_t _Index>
        inline typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::const_iterator
            find(const typename mk_nth_helper<_Index, _TLayer, _TLayers...>::type::key_type& _key) const
        {
            return std::get<_Index>(maps_).find(_key);
        }
        
        inline typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::iterator
            find(const typename mk_nth_helper<0, _TLayer, _TLayers...>::type::key_type& _key)
        {
            return std::get<0>(maps_).find(_key);
        }
        
        inline typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::const_iterator
            find(const typename mk_nth_helper<0, _TLayer, _TLayers...>::type::key_type& _key) const
        {
            return std::get<0>(maps_).find(_key);
        }

        template<size_t _Index>
        inline typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::iterator
            end()
        {
            return std::get<_Index>(maps_).end();
        }

        template<size_t _Index>
        inline typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::const_iterator
            end() const
        {
            return std::get<_Index>(maps_).end();
        }

        inline typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::iterator
            end()
        {
            return std::get<0>(maps_).end();
        }

        inline typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::const_iterator
            end() const
        {
            return std::get<0>(maps_).end();
        }
        
        template<size_t _Index>
        inline typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::iterator
            begin()
        {
            return std::get<_Index>(maps_).begin();
        }

        template<size_t _Index>
        inline typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::const_iterator
            begin() const
        {
            return std::get<_Index>(maps_).begin();
        }
        
        inline typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::iterator
            begin()
        {
            return std::get<0>(maps_).begin();
        }
        
        inline typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::const_iterator
            begin() const
        {
            return std::get<0>(maps_).begin();
        }
        
        template<size_t _Index>
        inline typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::iterator
            erase(typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::iterator _it)
        {
            if (_it == std::get<_Index>(maps_).end())
                return _it;
            erase_impl<_Index, sizeof...(_TLayers)>(_it->first.tuple());
            return std::get<_Index>(maps_).erase(_it);
        }

        template<size_t _Index>
        inline typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::iterator
            erase(typename mk_map_tuple<_THashMap, _TValue, _Index, _TLayer, _TLayers...>::map_type::const_iterator _it)
        {
            if (_it == std::get<_Index>(maps_).end())
                return _it;
            erase_impl<_Index, sizeof...(_TLayers)>(_it->first.tuple());
            return std::get<_Index>(maps_).erase(_it);
        }

        inline typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::iterator
            erase(typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::iterator _it)
        {
            if (_it == std::get<0>(maps_).end())
                return _it;
            erase_impl<0, sizeof...(_TLayers)>(_it->first.tuple());
            return std::get<0>(maps_).erase(_it);
        }

        inline typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::iterator
            erase(typename mk_map_tuple<_THashMap, _TValue, 0, _TLayer, _TLayers...>::map_type::const_iterator _it)
        {
            if (_it == std::get<0>(maps_).end())
                return _it;
            erase_impl<0, sizeof...(_TLayers)>(_it->first.tuple());
            return std::get<0>(maps_).erase(_it);
        }

        template<size_t _Index>
        inline size_t
            erase(const typename mk_nth_helper<_Index, _TLayer, _TLayers...>::type::key_type& _key)
        {
            auto it = std::get<_Index>(maps_).find(_key);
            if (it == std::get<_Index>(maps_).end())
                return 0;
            erase_impl<_Index, sizeof...(_TLayers)>(it->first.tuple());
            std::get<_Index>(maps_).erase(it);
            return 1;
        }
        
        inline size_t erase(const typename mk_nth_helper<0, _TLayer, _TLayers...>::type::key_type& _key)
        {
            auto it = std::get<0>(maps_).find(_key);
            if (it == std::get<0>(maps_).end())
                return 0;
            erase_impl<0, sizeof...(_TLayers)>(it->first.tuple());
            std::get<0>(maps_).erase(it);
            return 1;
        }

        inline void clear() noexcept
        {
            clear_impl<sizeof...(_TLayers)>();
        }

        inline bool insert(const mk_tuple<_TLayer, _TLayers...>& _key, const _TValue& _value)
        {
            if (exist_each_impl<sizeof...(_TLayers)>(_key))
                return false;
            return  insert_impl<sizeof...(_TLayers)>(_key, _value);
        }
        
    protected:

        template<size_t _Index>
        inline bool insert_impl(const mk_tuple<_TLayer, _TLayers...>& _key, const _TValue& _value)
        {
            return mk_details::inserter<_Index>::insert(maps_, _key, _value);
        }
        
        template<size_t _IgnoreIndex, size_t _Index>
        inline void erase_impl(const mk_tuple<_TLayer, _TLayers...>& _key)
        {
            mk_details::eraser<_IgnoreIndex, _Index>::erase(maps_, _key);
        }

        template<size_t _Index>
        inline void clear_impl() noexcept
        {
            mk_details::clearer<_Index>::clear(maps_);
        }

        template<size_t _Index>
        inline bool exist_each_impl(const mk_tuple<_TLayer, _TLayers...>& _key)
        {
            return mk_details::exist_eacher<_Index>::exist(maps_, _key);
        }
        
        map_tuple_type maps_;
    };

    template<typename _TValue, typename _TKey, typename... _TKeys>
    using mkhmap = mk_layered_hmap<std::unordered_map, _TValue, mkhmap_layer<_TKey>, mkhmap_layer<_TKeys>...>;
}    
};

#endif // !MMBKPP_CONTAINER_MKHMAP_HPP_INCLUDED
