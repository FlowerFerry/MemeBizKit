
#ifndef MMBKPP_CONTAINER_MKHMAP_HPP_INCLUDED
#define MMBKPP_CONTAINER_MKHMAP_HPP_INCLUDED

#include <unordered_map>
#include <tuple>

namespace mmbkpp {
namespace container {

    template<size_t _Index, typename _Ty, typename... _Ts>
    struct mkmain
    {
        using type = typename mkmain<_Index - 1, _Ts...>::type;
    };

    template<typename _Ty, typename... _Ts>
    struct mkmain<0, _Ty, _Ts...>
    {
        using type = _Ty;
    };

    template<typename _TValue, typename _TKey, typename... _TKeys>
    struct mkhmap;
    template<size_t _Index, typename _Ty, typename... _Ts>
    struct mk
    {
        mk() {}
        // TO_DO
        mk(const typename mkmain<_Index, _Ty, _Ts...>::type& _key)
        {
            std::get<_Index>(tuple_) = _key;
        }
        mk(const std::tuple<_Ty, _Ts...>& _tuple)
            : tuple_(_tuple)
        {
        }
        mk(const mk&) = default;
        mk(mk&&) = default;
        
        inline bool operator==(const mk& _key) const noexcept
        {
            return std::get<_Index>(tuple_) == std::get<_Index>(_key.tuple_);
        }

        inline const typename mkmain<_Index, _Ty, _Ts...>::type& key() const noexcept
        {
            return std::get<_Index>(tuple_);
        }

        template<size_t _CurrIndex>
        inline const typename mkmain<_CurrIndex, _Ty, _Ts...>::type& key() const noexcept
        {
            return std::get<_CurrIndex>(tuple_);
        }

        inline const std::tuple<_Ty, _Ts...>& tuple() const noexcept
        {
            return tuple_;
        }
        
        std::tuple<_Ty, _Ts...> tuple_;
    };

    template<size_t _Index, typename... _Ty>
    struct mkhash
    {
        inline std::size_t operator()(const mk<_Index, _Ty...>& _k) const
        {
            return std::hash<typename mkmain<_Index, _Ty...>::type>()(_k.key());
        }
    };

    template<typename _TValue, size_t _Index, typename _TKey, typename... _TKeys>
    struct mkhmap_member
    {
        using map_type = std::unordered_map<mk<_Index, _TKey, _TKeys...>, _TValue, mkhash<_Index, _TKey, _TKeys...>>;
        using type = decltype(std::tuple_cat(std::declval<typename mkhmap_member<_TValue, _Index - 1, _TKey, _TKeys...>::type>(), std::tuple<map_type>()));
    };

    template<typename _TValue, typename _TKey, typename... _TKeys>
    struct mkhmap_member<_TValue, 0, _TKey, _TKeys...>
    {
        using map_type = std::unordered_map<mk<0, _TKey, _TKeys...>, _TValue, mkhash<0, _TKey, _TKeys...>>;
        using type = std::tuple<map_type>;
    };

    namespace details {

        template<size_t _Index>
        struct ExistEacher {
            template<typename TMapTuple, typename... TKeys>
            static bool exist(const TMapTuple& _maps, const std::tuple<TKeys...>& _key) {
                return std::get<_Index>(_maps).find(std::get<_Index>(_key)) != std::get<_Index>(_maps).end()
                    || ExistEacher<_Index - 1>::exist(_maps, _key);
            }
        };

        template<>
        struct ExistEacher<0> {
            template<typename TMapTuple, typename... TKeys>
            static bool exist(const TMapTuple& _maps, const std::tuple<TKeys...>& _key) {
                return std::get<0>(_maps).find(std::get<0>(_key)) != std::get<0>(_maps).end();
            }
        };

        template<size_t _IgnoreIndex, size_t _Index>
        struct Eraser {
            template<typename TMapTuple, typename TKey, typename... TKeys>
            static void erase(TMapTuple& _maps, const std::tuple<TKey, TKeys...>& _key) {
                if (_Index != _IgnoreIndex) {
                    std::get<_Index>(_maps).erase(std::get<_Index>(_key));
                }
                Eraser<_IgnoreIndex, _Index - 1>::erase(_maps, _key);
            }
        };

        template<size_t _IgnoreIndex>
        struct Eraser<_IgnoreIndex, 0> {
            template<typename TMapTuple, typename TKey, typename... TKeys>
            static void erase(TMapTuple& _maps, const std::tuple<TKey, TKeys...>& _key) {
                if (0 != _IgnoreIndex) {
                    std::get<0>(_maps).erase(std::get<0>(_key));
                }
            }
        };

        template<size_t _Index>
        struct Inserter {
            template<typename TMapTuple, typename TValue, typename TKey, typename... TKeys>
            static bool insert(TMapTuple& _maps, const std::tuple<TKey, TKeys...>& _key, const TValue& _value)
            {
                if (std::get<_Index>(_maps).insert(std::make_pair(_key, _value)).second == false) {
                    Eraser<_Index, sizeof...(TKeys)>::erase(_maps, _key);
                    return false;
                }
                return Inserter<_Index - 1>::insert(_maps, _key, _value);
            }
        };

        template<>
        struct Inserter<0> {
            template<typename TMapTuple, typename _TValue, typename TKey, typename... TKeys>
            static bool insert(TMapTuple& _maps, const std::tuple<TKey, TKeys...>& _key, const _TValue& _value)
            {
                if (std::get<0>(_maps).insert(std::make_pair(_key, _value)).second == false) {
                    Eraser<0, sizeof...(TKeys)>::erase(_maps, _key);
                    return false;
                }
                return true;
            }
        };

    };
    
    template<typename _TValue, typename _TKey, typename... _TKeys>
    struct mkhmap
    {
        using map_tuple_t = typename mkhmap_member<_TValue, sizeof...(_TKeys), _TKey, _TKeys...>::type;
        
        using iterator = typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::iterator;
        using const_iterator = typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::const_iterator;

        inline bool empty() const noexcept
        {
            return std::get<0>(maps_).empty();
        }

        inline size_t size() const noexcept
        {
            return std::get<0>(maps_).size();
        }

        template<size_t _Index>
        inline typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::iterator
            find(const typename mkmain<_Index, _TKey, _TKeys...>::type& _key)
        {
            return std::get<_Index>(maps_).find(_key);
        }

        template<size_t _Index>
        inline typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::const_iterator
            find(const typename mkmain<_Index, _TKey, _TKeys...>::type& _key) const
        {
            return std::get<_Index>(maps_).find(_key);
        }
        
        inline typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::iterator
            find(const typename mkmain<0, _TKey, _TKeys...>::type& _key)
        {
            return std::get<0>(maps_).find(_key);
        }
        
        inline typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::const_iterator
            find(const typename mkmain<0, _TKey, _TKeys...>::type& _key) const
        {
            return std::get<0>(maps_).find(_key);
        }

        template<size_t _Index>
        inline typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::iterator
            end()
        {
            return std::get<_Index>(maps_).end();
        }

        template<size_t _Index>
        inline typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::const_iterator
            end() const
        {
            return std::get<_Index>(maps_).end();
        }

        inline typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::iterator
            end()
        {
            return std::get<0>(maps_).end();
        }

        inline typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::const_iterator
            end() const
        {
            return std::get<0>(maps_).end();
        }
        
        template<size_t _Index>
        inline typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::iterator
            begin()
        {
            return std::get<_Index>(maps_).begin();
        }

        template<size_t _Index>
        inline typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::const_iterator
            begin() const
        {
            return std::get<_Index>(maps_).begin();
        }
        
        inline typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::iterator
            begin()
        {
            return std::get<0>(maps_).begin();
        }
        
        inline typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::const_iterator
            begin() const
        {
            return std::get<0>(maps_).begin();
        }
        
        template<size_t _Index>
        inline typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::iterator
            erase(typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::iterator _it)
        {
            if (_it == std::get<_Index>(maps_).end())
                return _it;
            erase_impl<_Index, sizeof...(_TKeys)>(_it->first.tuple());
            return std::get<_Index>(maps_).erase(_it);
        }

        template<size_t _Index>
        inline typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::iterator
            erase(typename mkhmap_member<_TValue, _Index, _TKey, _TKeys...>::map_type::const_iterator _it)
        {
            if (_it == std::get<_Index>(maps_).end())
                return _it;
            erase_impl<_Index, sizeof...(_TKeys)>(_it->first.tuple());
            return std::get<_Index>(maps_).erase(_it);
        }

        inline typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::iterator
            erase(typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::iterator _it)
        {
            if (_it == std::get<0>(maps_).end())
                return _it;
            erase_impl<0, sizeof...(_TKeys)>(_it->first.tuple());
            return std::get<0>(maps_).erase(_it);
        }

        inline typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::iterator
            erase(typename mkhmap_member<_TValue, 0, _TKey, _TKeys...>::map_type::const_iterator _it)
        {
            if (_it == std::get<0>(maps_).end())
                return _it;
            erase_impl<0, sizeof...(_TKeys)>(_it->first.tuple());
            return std::get<0>(maps_).erase(_it);
        }

        template<size_t _Index>
        inline size_t
            erase(const typename mkmain<_Index, _TKey, _TKeys...>::type& _key)
        {
            auto it = std::get<_Index>(maps_).find(_key);
            if (it == std::get<_Index>(maps_).end())
                return 0;
            erase_impl<_Index, sizeof...(_TKeys)>(it->first.tuple());
            return std::get<_Index>(maps_).erase(_key);
        }
        
        inline size_t erase(const typename mkmain<0, _TKey, _TKeys...>::type& _key)
        {
            auto it = std::get<0>(maps_).find(_key);
            if (it == std::get<0>(maps_).end())
                return 0;
            erase_impl<0, sizeof...(_TKeys)>(it->first.tuple());
            return std::get<0>(maps_).erase(_key);
        }

        inline bool insert(const std::tuple<_TKey, _TKeys...>& _key, const _TValue& _value)
        {
            if (exist_each_impl<sizeof...(_TKeys)>(_key))
                return false;
            return  insert_impl<sizeof...(_TKeys)>(_key, _value);
        }
        
    protected:

        template<size_t _Index>
        inline bool insert_impl(const std::tuple<_TKey, _TKeys...>& _key, const _TValue& _value)
        {
            return details::Inserter<_Index>::insert(maps_, _key, _value);
        }
        
        template<size_t _IgnoreIndex, size_t _Index>
        inline void erase_impl(const std::tuple<_TKey, _TKeys...>& _key)
        {
            details::Eraser<_IgnoreIndex, _Index>::erase(maps_, _key);
        }
        
        template<size_t _Index>
        inline bool exist_each_impl(const std::tuple<_TKey, _TKeys...>& _key)
        {
            return details::ExistEacher<_Index>::exist(maps_, _key);
        }
        
        map_tuple_t maps_;
    };
}    
};

#endif // !MMBKPP_CONTAINER_MKHMAP_HPP_INCLUDED
