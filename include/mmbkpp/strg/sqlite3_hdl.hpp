
#ifndef MMBKPP_STRG_SQLITE3_HDL_HPP_INCLUDED
#define MMBKPP_STRG_SQLITE3_HDL_HPP_INCLUDED

#include <sqlite3.h>
#include <mego/predef/symbol/likely.h>
#include <mego/err/sqlite3_convert.h>

#include <megopp/err/err.hpp>
#include <megopp/util/scope_cleanup.h>
#include <outcome/result.hpp>

#include <type_traits>
#include <functional>

namespace outcome = OUTCOME_V2_NAMESPACE;

#include <memory>

namespace mmbkpp { namespace strg {

struct sqlite3_hdl
{
    typedef void(*close_cb_t)(const std::shared_ptr<void>&);

    sqlite3_hdl() = delete;
    sqlite3_hdl(const sqlite3_hdl&) = delete;
    sqlite3_hdl(sqlite3_hdl&&) = default;
    sqlite3_hdl& operator=(const sqlite3_hdl&) = delete;
    sqlite3_hdl& operator=(sqlite3_hdl&&) = default;

    ~sqlite3_hdl() noexcept
    {
        if (MEGO_SYMBOL__LIKELY(hdl_ != nullptr))
            ::sqlite3_close(hdl_);

        if (on_close_) {
            on_close_(userdata_);
        }
    }

    inline std::shared_ptr<void> userdata() const noexcept
    {
        return userdata_;
    }

    inline void set_close_cb(close_cb_t cb) noexcept
    {
        on_close_ = cb;
    }

    inline void set_userdata(const std::shared_ptr<void>& userdata) noexcept
    {
        userdata_ = userdata;
    }

    template<typename _Fn>
    mgpp::err do_read(const char* _sql, _Fn&& _fn);

    mgpp::err do_write(const char* _sql);

    template<typename _Fn>
    mgpp::err do_writes(_Fn&& _fn);

    inline constexpr ::sqlite3* native() const noexcept { return hdl_; }

    inline static std::unique_ptr<sqlite3_hdl> take(::sqlite3* hdl) noexcept
    {
        return std::unique_ptr<sqlite3_hdl>(new sqlite3_hdl(hdl));
    }

    inline static std::shared_ptr<sqlite3_hdl> take_to_shared(::sqlite3* hdl) noexcept
    {
        return std::shared_ptr<sqlite3_hdl>(new sqlite3_hdl(hdl));
    }

    static outcome::checked<std::unique_ptr<sqlite3_hdl>, mgpp::err> 
        open(const char* filename, int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    static outcome::checked<std::shared_ptr<sqlite3_hdl>, mgpp::err> 
        open_to_shared(const char* filename, int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);

private:
    sqlite3_hdl(::sqlite3* _hdl) noexcept 
        : hdl_(_hdl) 
        , on_close_(nullptr)
    {}

    template <typename _Fn>
    struct __exec_ctx
    {
        __exec_ctx(_Fn&& _fn) noexcept
            : fn_(std::forward<_Fn>(_fn))
        {}

        static int exec_cb(void* ctx, int col_count, char** col_values, char** col_names)
        {
            auto p = static_cast<__exec_ctx*>(ctx);
            return p->fn_(col_count, col_values, col_names);
        }

        _Fn fn_;
    };

    ::sqlite3* hdl_;
    close_cb_t on_close_;
    std::shared_ptr<void> userdata_;
};
using sqlite3_hdl_uptr = std::unique_ptr<sqlite3_hdl>;
using sqlite3_hdl_sptr = std::shared_ptr<sqlite3_hdl>;

template <typename _Fn>
inline mgpp::err sqlite3_hdl::do_read(const char* _sql, _Fn && _fn)
{
    if (MEGO_SYMBOL__UNLIKELY(hdl_ == nullptr)) {
        return mgpp::err{ MGEC__ERR, "invalid sqlite3 hdl" };
    }

    char* errmsg = nullptr;
    MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { ::sqlite3_free(errmsg); });
    __exec_ctx<_Fn> ctx(std::forward<_Fn>(_fn));
    int rc = ::sqlite3_exec(hdl_, _sql, &__exec_ctx<_Fn>::exec_cb, &ctx, &errmsg);
    if (MEGO_SYMBOL__UNLIKELY(rc != SQLITE_OK)) {
        return mgpp::err{ mgec__from_sqlite3_err(rc), errmsg };
    }

    return mgpp::err::make_ok();
}

inline mgpp::err sqlite3_hdl::do_write(const char* _sql)
{
    if (MEGO_SYMBOL__UNLIKELY(hdl_ == nullptr)) {
        return mgpp::err{ MGEC__ERR, "invalid sqlite3 hdl" };
    }

    if (sqlite3_db_readonly(hdl_, nullptr) == 1) {
        return mgpp::err{ MGEC__ERR, "sqlite3 hdl is readonly" };
    }

    char* errmsg = nullptr;
    MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { ::sqlite3_free(errmsg); });
    int rc = ::sqlite3_exec(hdl_, _sql, nullptr, nullptr, &errmsg);
    if (MEGO_SYMBOL__UNLIKELY(rc != SQLITE_OK)) {
        return mgpp::err{ mgec__from_sqlite3_err(rc), errmsg };
    }

    return mgpp::err::make_ok();
}

template <typename _Fn>
inline mgpp::err sqlite3_hdl::do_writes(_Fn && _fn)
{
    if (MEGO_SYMBOL__UNLIKELY(hdl_ == nullptr)) {
        return mgpp::err{ MGEC__ERR, "invalid sqlite3 hdl" };
    }

    if (sqlite3_db_readonly(hdl_, nullptr) == 1) {
        return mgpp::err{ MGEC__ERR, "sqlite3 hdl is readonly" };
    }

    char* errmsg = nullptr;
    MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] { ::sqlite3_free(errmsg); });
    int rc = ::sqlite3_exec(hdl_, "BEGIN TRANSACTION", nullptr, nullptr, &errmsg);
    if (MEGO_SYMBOL__UNLIKELY(rc != SQLITE_OK)) {
        return mgpp::err{ mgec__from_sqlite3_err(rc), errmsg };
    }
    auto cleanup = megopp::util::scope_cleanup__create(
        [&] { ::sqlite3_exec(hdl_, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr); });

    auto e = _fn(hdl_);
    if (MEGO_SYMBOL__UNLIKELY(e.ok() == false)) {
        return e;
    }

    rc = ::sqlite3_exec(hdl_, "COMMIT TRANSACTION", nullptr, nullptr, &errmsg);
    if (MEGO_SYMBOL__UNLIKELY(rc != SQLITE_OK)) {
        return mgpp::err{ mgec__from_sqlite3_err(rc), errmsg };
    }

    cleanup.cancel();
    return mgpp::err::make_ok();
}

inline outcome::checked<std::unique_ptr<sqlite3_hdl>, mgpp::err> 
    sqlite3_hdl::open(const char* filename, int flags)
{
    ::sqlite3* hdl = nullptr;
    int rc = ::sqlite3_open_v2(filename, &hdl, flags, nullptr);
    if (MEGO_SYMBOL__LIKELY(rc == SQLITE_OK))
        return take(hdl);
    return mgpp::err{ mgec__from_sqlite3_err(rc) };
}

inline outcome::checked<std::shared_ptr<sqlite3_hdl>, mgpp::err>
    sqlite3_hdl::open_to_shared(const char* filename, int flags)
{
    ::sqlite3* hdl = nullptr;
    int rc = ::sqlite3_open_v2(filename, &hdl, flags, nullptr);
    if (MEGO_SYMBOL__LIKELY(rc == SQLITE_OK))
        return take_to_shared(hdl);
    return mgpp::err{ mgec__from_sqlite3_err(rc) };
}

} } // namespace mmbkpp

#endif // !MMBKPP_STRG_SQLITE3_HDL_HPP_INCLUDED

