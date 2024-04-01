
#ifndef MMBKPP_CONTAINER_DKMAP_HPP_INCLUDED
#define MMBKPP_CONTAINER_DKMAP_HPP_INCLUDED

#include <memepp/string.hpp>
#include <memepp/hash/std/hash.hpp>

#include <unordered_map>

namespace mmbkpp {
namespace container {

    struct dk
    {
        inline bool operator==(const dk& _Right) const noexcept
        {
            return fkey == _Right.fkey && skey == _Right.skey;
        }

        memepp::string fkey;
        memepp::string skey;
    };

}
}

namespace std {
    template<>
    struct hash<mmbkpp::container::dk>
    {
        size_t operator()(const mmbkpp::container::dk& _Keyval) const
        {
            return std::hash<memepp::string>()(_Keyval.fkey) 
                 ^ std::hash<memepp::string>()(_Keyval.skey);
        }
    };
}

namespace mmbkpp {
namespace container {

    template<
        typename _FKty, 
        typename _SKty, 
        typename _Value,
        typename _MHash = std::hash<dk>,
        typename _FHash = std::hash<_FKty>,
        typename _MAlloc = std::allocator<std::pair<const dk, _Value>>,
        typename _FAlloc = std::allocator<std::pair<const _FKty, dk>>
    >
    class dkmap 
    {
    public:
        using map_t = std::unordered_map<dk, _Value, _MHash, std::equal_to<dk>, _MAlloc>;
        using fk_t  = std::unordered_map<_FKty, dk, _FHash, std::equal_to<_FKty>, _FAlloc>;
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

        inline bool contains(const _FKty& _Keyval) const noexcept
        {
            return fks_.find(_Keyval) != fks_.end();
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

        inline iterator find(const _FKty& _Keyval) noexcept
        {
            auto it = fks_.find(_Keyval);
            if (it != fks_.end())
                return maps_.find(it->second);
            return maps_.end();
        }

        inline const_iterator find(const _FKty& _Keyval) const noexcept
        {
            auto it = fks_.find(_Keyval);
            if (it != fks_.end())
                return maps_.find(it->second);
            return maps_.end();
        }

        inline iterator erase(const dk& _Keyval) noexcept
        {
            fks_.erase(fks_.find(_Keyval.fkey));
            return maps_.erase(_Keyval);
        }

        inline iterator erase(const _FKty& _Keyval) noexcept
        {
            auto it1 = fks_.find(_Keyval);
            if (it1 != fks_.end())
            {
                auto it2 = maps_.find(it1->second);
                if (it2 != maps_.end())
                {
                    fks_.erase(it1);
                    return maps_.erase(it2);
                }
            }
            return maps_.end();
        }

        inline iterator insert(const dk& _Keyval, const _Value& _Val) noexcept
        {
            auto it = maps_.insert({ _Keyval, _Val });
            if (it.second)
                fks_.insert({ _Keyval.fkey, _Keyval });
            return it.first;
        }

        inline iterator insert(const _FKty& _Keyval1, const _SKty& _Keyval2, const _Value& _Val) noexcept
        {
            dk keyval = { _Keyval1, _Keyval2 };
            auto it = maps_.insert({ keyval, _Val });
            if (it.second)
                fks_.insert({ _Keyval1, keyval });
            return it.first;
        }

    private:
        map_t maps_;
        fk_t  fks_;
    };

}
}

#endif // !MMBKPP_CONTAINER_DKMAP_HPP_INCLUDED
