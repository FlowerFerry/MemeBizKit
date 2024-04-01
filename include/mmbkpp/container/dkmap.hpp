
#ifndef MMBKPP_CONTAINER_DKMAP_HPP_INCLUDED
#define MMBKPP_CONTAINER_DKMAP_HPP_INCLUDED

#include <memepp/string.hpp>
#include <memepp/hash/std/hash.hpp>

#include <unordered_set>
#include <unordered_map>

namespace mmbkpp {
namespace container {

    template<typename _FKty, typename _SKty>
    struct dk
    {
        inline bool operator==(const dk& _Right) const noexcept
        {
            return fkey == _Right.fkey && skey == _Right.skey;
        }

        _FKty fkey;
        _SKty skey;
    };

}
}

namespace std {
    template<typename _FKty, typename _SKty>
    struct hash<mmbkpp::container::dk<_FKty, _SKty>>
    {
        size_t operator()(const mmbkpp::container::dk<_FKty, _SKty>& _Keyval) const
        {
            return std::hash<_FKty>()(_Keyval.fkey) 
                 ^ std::hash<_SKty>()(_Keyval.skey);
        }
    };
}

namespace mmbkpp {
namespace container {

    template<
        typename _FKty, 
        typename _SKty, 
        typename _Value,
        typename _MHash = std::hash<dk<_FKty, _SKty>>,
        typename _FHash = std::hash<_FKty>,
        typename _MAlloc = std::allocator<std::pair<const dk<_FKty, _SKty>, _Value>>,
        typename _FAlloc = std::allocator<std::pair<const _FKty, std::unordered_set<dk<_FKty, _SKty>>>>
    >
    class dkmap 
    {
    public:
        using map_t = std::unordered_map<dk<_FKty, _SKty>, _Value, _MHash, std::equal_to<dk>, _MAlloc>;
        using fk_t  = std::unordered_map<_FKty, std::unordered_set<dk<_FKty, _SKty>>, _FHash, std::equal_to<_FKty>, _FAlloc>;
        using iterator = typename map_t::iterator;
        using const_iterator = typename map_t::const_iterator;

        inline bool empty() const noexcept
        {
            return maps_.empty();
        }

        inline size_t size() const noexcept
        {
            return maps_.size();
        }

        inline void clear() noexcept
        {
            maps_.clear();
            fks_.clear();
        }

        inline bool contains(const dk& _Keyval) const noexcept
        {
            return maps_.find(_Keyval) != maps_.end();
        }

        inline iterator begin() noexcept
        {
            return maps_.begin();
        }

        inline const_iterator begin() const noexcept
        {
            return maps_.begin();
        }

        inline iterator end() noexcept
        {
            return maps_.end();
        }

        inline const_iterator end() const noexcept
        {
            return maps_.end();
        }

        inline iterator find(const dk& _Keyval) noexcept
        {
            return maps_.find(_Keyval);
        }

        inline const_iterator find(const dk& _Keyval) const noexcept
        {
            return maps_.find(_Keyval);
        }

        inline iterator erase(const dk& _Keyval) noexcept
        {
            auto it = maps_.find(_Keyval);
            if (it != maps_.end()) {
                fks_[_Keyval.fkey].erase(_Keyval);
                return maps_.erase(it);
            }
        }

        inline iterator insert(const dk& _Keyval, const _Value& _Val) noexcept
        {
            auto it = maps_.insert({ _Keyval, _Val });
            if (it.second) {
                fks_[_Keyval.fkey].insert(_Keyval); 
            }
            return it.first;
        }

        inline iterator insert(const _FKty& _Keyval1, const _SKty& _Keyval2, const _Value& _Val) noexcept
        {
            dk keyval = { _Keyval1, _Keyval2 };
            auto it = maps_.insert({ keyval, _Val });
            if (it.second)
                fks_[_Keyval1].insert(keyval);
            return it.first;
        }

    private:
        map_t maps_;
        fk_t  fks_;
    };

}
}

#endif // !MMBKPP_CONTAINER_DKMAP_HPP_INCLUDED
