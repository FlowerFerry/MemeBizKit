
#ifndef MMBKPP_I18N_CATALOG_H_INCLUDED
#define MMBKPP_I18N_CATALOG_H_INCLUDED

// =============================================================================
// mmbkpp/i18n/catalog.h - Header-only i18n translation library (C++17)
//
// [Purpose]
//   Provides gettext-style translation interfaces for host programs and plugins:
//     tr (plain text), tr_ctx (with context), tr_n (plural), tr_ctx_n (plural with context).
//   Each entry is uniquely identified by (language, context, msgid). Plural entries
//   use the singular msgid as the key, with multiple forms stored as form = 0..k-1.
//
// [Two modes] Determined at construction time, not switchable at runtime
//   1. fast_catalog: Loaded into contiguous memory by fast_catalog_builder at startup.
//      After build(), data is immutable; query path is lock-free and allocation-free
//      (except for return value construction). Long translations use MemeString's
//      takeover mechanism for zero-copy returns. Typically used by the host program.
//   2. lite_catalog (macro MMBKPP_I18N_ENABLE_SQLITE, default 1):
//      Does not load entries; each query directly accesses the SQLite table.
//      One read-only connection per thread; connection closes on thread exit.
//      Optional per-thread LRU cache with configurable capacity.
//      Typically used for plugin-associated SQLite files.
//
// [Language tags] Always use mmupp::locale::simple_tag (mmutilspp/locale/tag.h).
//   Tags must be constructed via make_tag(). Passing string literals directly to
//   tag parameters causes silent truncation and invalid tags. Therefore, all
//   tag-accepting interfaces provide = delete overloads for string types to
//   make such misuse a compile-time error.
//
// [Language fallback chain] Request tag t -> [t, t.to_lang_region_only(), t.to_lang_only()]
//   (duplicates removed). Processed in order: stops immediately if source_language
//   is encountered (treated as miss); hits if the entry exists in a candidate language.
//   Does NOT fall back to default language (to avoid "requested Japanese but got Chinese"
//   mixed-language scenarios). The original text itself is the universal fallback.
//
// [Miss behavior] tr / tr_ctx return msgid; tr_n / tr_ctx_n return
//   n == 1 ? msgid : msgid_plural.
//
// [Thread safety]
//   fast_catalog_builder  Not thread-safe; single-threaded construction.
//   fast_catalog          All const members (including queries) are thread-safe;
//                         setters are thread-safe.
//   lite_catalog          Queries are thread-safe; destructor must not run concurrently
//                         with calls (catalog_registry's snapshot mechanism guarantees this).
//   catalog_registry      Fully thread-safe.
//   global / set_global   Thread-safe. global() returns a std::shared_ptr, so a
//                         registry stays alive for the whole duration of a query
//                         even if another thread calls set_global() meanwhile.
//
// [Cross-DLL prerequisites (important)]
//   Long translations returned by fast_catalog are backed by reference-counted memory
//   pool blocks. The release callback (blk->release()) references code in the host
//   module and runs in the host module. Therefore, the host and plugins must link
//   **the same MemeString dynamic library** (same allocator), and the host module
//   must remain resident. When this prerequisite is met, strings held externally
//   remain valid after the catalog is removed/destroyed; the pool block is released
//   only when the last string copy is destroyed.
//
// [lite mode destruction constraints]
//   When destroying lite_catalog, no other thread may be calling it; destruction
//   closes all connections and releases the database file handle. Host plugin
//   unload procedure:
//     auto tk = reg.remove("x"); reset your own shared_ptr;
//     after tk.wait_released(...) returns true, unload the DLL and delete/move the db file.
//
// [DB table structure (shared by both modes)]
//   CREATE TABLE i18n_language (
//       lang         TEXT PRIMARY KEY,          -- e.g. 'zh-CN'
//       plural_rule  TEXT NOT NULL DEFAULT ''   -- '' / '@ru' / 'nplurals=..; plural=..;'
//   ) WITHOUT ROWID;
//   CREATE TABLE i18n_message (
//       lang    TEXT    NOT NULL,
//       ctx     TEXT    NOT NULL DEFAULT '',
//       msgid   TEXT    NOT NULL,
//       form    INTEGER NOT NULL DEFAULT 0,
//       msgstr  TEXT    NOT NULL,
//       PRIMARY KEY (lang, ctx, msgid, form)
//   ) WITHOUT ROWID;
//   i18n_message.lang must be byte-identical to i18n_language.lang.
//
// [Known limits]
//   - fast_catalog: a single language's string pool is capped at 4 GiB (offsets
//     are uint32_t). build() throws std::length_error if that limit is exceeded.
//   - lite_catalog: destruction must not run concurrently with queries; use
//     catalog_registry::remove() + release_token::wait_released() to sequence it.
//   - global() returns a shared_ptr, so a concurrent set_global() cannot destroy
//     the registry while a query is in flight; the caller holds a strong
//     reference for the duration of the call.
// =============================================================================

#ifndef MMBKPP_I18N_ENABLE_SQLITE
#define MMBKPP_I18N_ENABLE_SQLITE 1
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <memepp/string.hpp>
#include <memepp/string_view.hpp>
#include <memepp/convert/std/c_str.hpp>
#include <megopp/memory/atomic_shared_ptr.h>
#include <megopp/thrd/seqlock.h>
#include <megopp/err/err.hpp>
#include <mego/err/ec.h>
#include <mmutilspp/locale/tag.h>

#if MMBKPP_I18N_ENABLE_SQLITE
#include <sqlite3.h>
#include <mego/err/convert/sqlite3.h>
#endif

namespace mmbkpp {
namespace i18n {

using tag_t = mmupp::locale::simple_tag;

//! @brief Parse "zh-CN" / "zh_Hans_CN" etc. into a locale tag.
inline tag_t make_tag(const memepp::string_view& _s)
{
    return tag_t::parse_from(_s);
}

namespace details {

// ---------------------------------------------------------------------------
// FNV-1a (64-bit, incremental)
// ---------------------------------------------------------------------------
struct fnv1a
{
    uint64_t h = 14695981039346656037ull;

    void feed(const void* _p, size_t _n) noexcept
    {
        const unsigned char* b = static_cast<const unsigned char*>(_p);
        for (size_t i = 0; i < _n; ++i) {
            h ^= b[i];
            h *= 1099511628211ull;
        }
    }

    void feed_byte(unsigned char _c) noexcept
    {
        h ^= _c;
        h *= 1099511628211ull;
    }
};

//! @brief Hash of the (ctx, msgid) pair; 0x04 is the field separator so that
//!        ("ab","c") and ("a","bc") hash differently.
inline uint64_t key_hash(const memepp::string_view& _ctx, const memepp::string_view& _id) noexcept
{
    fnv1a f;
    if (_ctx.size() > 0)
        f.feed(_ctx.data(), static_cast<size_t>(_ctx.size()));
    f.feed_byte(0x04);
    if (_id.size() > 0)
        f.feed(_id.data(), static_cast<size_t>(_id.size()));
    return f.h;
}

//! @brief Number of live fast-mode string pool blocks (ctor +1, dtor -1). For tests.
inline std::atomic<long>& live_pool_blocks() noexcept
{
    static std::atomic<long> c{ 0 };
    return c;
}

// ---------------------------------------------------------------------------
// Language fallback chain
// ---------------------------------------------------------------------------
struct tag_chain
{
    tag_t v[3];
    int   n = 0;
};

inline tag_chain make_chain(const tag_t& _t) noexcept
{
    tag_chain c;
    if (!_t.valid())
        return c;

    const tag_t cand[3] = { _t, _t.to_lang_region_only(), _t.to_lang_only() };
    for (int i = 0; i < 3; ++i) {
        if (c.n == 0 || !(c.v[c.n - 1] == cand[i]))
            c.v[c.n++] = cand[i];
    }
    return c;
}

} // namespace details

// ---------------------------------------------------------------------------
// Plural rules
// ---------------------------------------------------------------------------
enum class plural_family : uint8_t
{
    one, en, fr, ru, pl, cs, lt, lv, ro, sl, ga, ar,
    expr   //!< parsed by the gettext expression interpreter
};

namespace details {

//! @brief Evaluate one of the built-in rule families (GNU gettext formulas).
inline uint32_t builtin_index(plural_family _f, uint64_t _n) noexcept
{
    switch (_f) {
    case plural_family::one: return 0;
    case plural_family::en:  return _n != 1 ? 1u : 0u;
    case plural_family::fr:  return _n > 1 ? 1u : 0u;
    case plural_family::ru:
        return _n % 10 == 1 && _n % 100 != 11 ? 0u
             : _n % 10 >= 2 && _n % 10 <= 4 && (_n % 100 < 10 || _n % 100 >= 20) ? 1u : 2u;
    case plural_family::pl:
        return _n == 1 ? 0u
             : _n % 10 >= 2 && _n % 10 <= 4 && (_n % 100 < 10 || _n % 100 >= 20) ? 1u : 2u;
    case plural_family::cs:  return _n == 1 ? 0u : (_n >= 2 && _n <= 4) ? 1u : 2u;
    case plural_family::lt:
        return _n % 10 == 1 && _n % 100 != 11 ? 0u
             : _n % 10 >= 2 && (_n % 100 < 10 || _n % 100 >= 20) ? 1u : 2u;
    case plural_family::lv:  return _n % 10 == 1 && _n % 100 != 11 ? 0u : _n != 0 ? 1u : 2u;
    case plural_family::ro:  return _n == 1 ? 0u
             : (_n == 0 || (_n % 100 > 0 && _n % 100 < 20)) ? 1u : 2u;
    case plural_family::sl:
        return _n % 100 == 1 ? 0u : _n % 100 == 2 ? 1u : (_n % 100 == 3 || _n % 100 == 4) ? 2u : 3u;
    case plural_family::ga:
        return _n == 1 ? 0u : _n == 2 ? 1u : _n < 7 ? 2u : _n < 11 ? 3u : 4u;
    case plural_family::ar:
        return _n == 0 ? 0u : _n == 1 ? 1u : _n == 2 ? 2u
             : (_n % 100 >= 3 && _n % 100 <= 10) ? 3u : _n % 100 >= 11 ? 4u : 5u;
    default: return _n != 1 ? 1u : 0u;
    }
}

//! @brief Number of plural forms of each built-in family.
inline uint32_t builtin_nplurals(plural_family _f) noexcept
{
    switch (_f) {
    case plural_family::one: return 1;
    case plural_family::sl:  return 4;
    case plural_family::ga:  return 5;
    case plural_family::ar:  return 6;
    case plural_family::en: case plural_family::fr: return 2;
    default: return 3;
    }
}

//! @brief AST node of the gettext plural expression.
struct plural_node
{
    enum op_t : uint8_t {
        num, var_n, not_, mul, div, mod, add, sub,
        lt, gt, le, ge, eq, ne, and_, or_, cond
    };

    op_t     op;
    uint64_t val = 0;          //!< for num
    int32_t  a = -1, b = -1, c = -1;
};

struct plural_program
{
    std::vector<plural_node> nodes;
    int32_t  root = -1;
    uint32_t nplurals = 2;

    //! @brief Recursive evaluation; nesting depth was limited to 32 at parse time.
    uint64_t eval(int32_t _i, uint64_t _n) const noexcept
    {
        if (_i < 0 || static_cast<size_t>(_i) >= nodes.size())
            return 0;
        const plural_node& nd = nodes[static_cast<size_t>(_i)];
        switch (nd.op) {
        case plural_node::num:   return nd.val;
        case plural_node::var_n: return _n;
        case plural_node::not_:  return eval(nd.a, _n) ? 0ull : 1ull;
        case plural_node::mul:   return eval(nd.a, _n) * eval(nd.b, _n);
        case plural_node::div:   { uint64_t d = eval(nd.b, _n); return d ? eval(nd.a, _n) / d : 0ull; }
        case plural_node::mod:   { uint64_t d = eval(nd.b, _n); return d ? eval(nd.a, _n) % d : 0ull; }
        case plural_node::add:   return eval(nd.a, _n) + eval(nd.b, _n);
        case plural_node::sub:   return eval(nd.a, _n) - eval(nd.b, _n);
        case plural_node::lt:    return eval(nd.a, _n) <  eval(nd.b, _n) ? 1ull : 0ull;
        case plural_node::gt:    return eval(nd.a, _n) >  eval(nd.b, _n) ? 1ull : 0ull;
        case plural_node::le:    return eval(nd.a, _n) <= eval(nd.b, _n) ? 1ull : 0ull;
        case plural_node::ge:    return eval(nd.a, _n) >= eval(nd.b, _n) ? 1ull : 0ull;
        case plural_node::eq:    return eval(nd.a, _n) == eval(nd.b, _n) ? 1ull : 0ull;
        case plural_node::ne:    return eval(nd.a, _n) != eval(nd.b, _n) ? 1ull : 0ull;
        case plural_node::and_:  return eval(nd.a, _n) ? (eval(nd.b, _n) ? 1ull : 0ull) : 0ull;
        case plural_node::or_:   return eval(nd.a, _n) ? 1ull : (eval(nd.b, _n) ? 1ull : 0ull);
        case plural_node::cond:  return eval(nd.a, _n) ? eval(nd.b, _n) : eval(nd.c, _n);
        }
        return 0;
    }
};

//! @brief Recursive-descent compiler for "nplurals=<n>; plural=<expr>;".
//!        C operator precedence, all arithmetic in uint64_t, short-circuit
//!        && || ?:, no unary minus.
class plural_compiler
{
public:
    static constexpr uint32_t k_max_depth = 32;
    static constexpr uint32_t k_max_nodes = 256;

    explicit plural_compiler(const memepp::string_view& _s) noexcept
        : p_(_s.data())
        , n_(static_cast<size_t>(_s.size()))
    {}

    bool compile()
    {
        skip_ws();
        if (!expect_word("nplurals"))
            return fail("expected 'nplurals'");
        skip_ws();
        if (!expect_char('='))
            return fail("expected '=' after 'nplurals'");
        skip_ws();

        uint64_t k = 0;
        if (!read_uint(k))
            return fail("expected nplurals number");
        if (k < 1 || k > 16)
            return fail("nplurals out of range");
        nplurals_ = static_cast<uint32_t>(k);

        skip_ws();
        if (!expect_char(';'))
            return fail("expected ';'");
        skip_ws();
        if (!expect_word("plural"))
            return fail("expected 'plural'");
        skip_ws();
        if (!expect_char('='))
            return fail("expected '=' after 'plural'");

        root_ = parse_expr();
        if (root_ < 0)
            return false;

        skip_ws();
        if (i_ < n_ && p_[i_] == ';') {
            ++i_;
            skip_ws();
        }
        if (i_ != n_)
            return fail("trailing characters");

        return true;
    }

    uint32_t nplurals() const noexcept { return nplurals_; }
    int32_t  root()     const noexcept { return root_; }
    const char* error() const noexcept { return err_; }

    std::vector<plural_node>& nodes() noexcept { return nodes_; }

private:
    struct depth_guard
    {
        uint32_t& d;
        bool      ok;
        explicit depth_guard(uint32_t& _d) : d(_d), ok(++_d <= k_max_depth) {}
        ~depth_guard() { --d; }
    };

    static bool is_digit(char _c) noexcept { return _c >= '0' && _c <= '9'; }
    static bool is_space(char _c) noexcept
    {
        return _c == ' ' || _c == '\t' || _c == '\r' || _c == '\n' || _c == '\f' || _c == '\v';
    }

    void skip_ws() noexcept
    {
        while (i_ < n_ && is_space(p_[i_]))
            ++i_;
    }

    bool fail(const char* _m)
    {
        if (err_[0] == '\0')
            err_ = _m;
        return false;
    }

    bool expect_char(char _c) noexcept
    {
        if (i_ < n_ && p_[i_] == _c) {
            ++i_;
            return true;
        }
        return false;
    }

    bool expect_word(const char* _w) noexcept
    {
        const size_t len = std::strlen(_w);
        if (i_ + len > n_)
            return false;
        if (std::memcmp(p_ + i_, _w, len) != 0)
            return false;
        i_ += len;
        return true;
    }

    bool read_uint(uint64_t& _v)
    {
        if (i_ >= n_ || !is_digit(p_[i_]))
            return false;
        uint64_t r = 0;
        while (i_ < n_ && is_digit(p_[i_])) {
            const uint64_t d = static_cast<uint64_t>(p_[i_] - '0');
            if (r > (UINT64_MAX - d) / 10) {
                fail("number overflow");
                return false;
            }
            r = r * 10 + d;
            ++i_;
        }
        _v = r;
        return true;
    }

    bool peek2(char _a, char _b) const noexcept
    {
        return i_ + 1 < n_ && p_[i_] == _a && p_[i_ + 1] == _b;
    }

    int32_t add_node(plural_node::op_t _op, uint64_t _val,
                     int32_t _a, int32_t _b, int32_t _c)
    {
        if (nodes_.size() >= k_max_nodes) {
            fail("too many nodes");
            return -1;
        }
        plural_node nd;
        nd.op = _op;
        nd.val = _val;
        nd.a = _a;
        nd.b = _b;
        nd.c = _c;
        nodes_.push_back(nd);
        return static_cast<int32_t>(nodes_.size() - 1);
    }

    // expr := or ( '?' expr ':' expr )?      -- right associative
    int32_t parse_expr()
    {
        depth_guard g(depth_);
        if (!g.ok) {
            fail("expression too deep");
            return -1;
        }

        const int32_t cond = parse_or();
        if (cond < 0)
            return -1;

        skip_ws();
        if (i_ < n_ && p_[i_] == '?') {
            ++i_;
            skip_ws();
            const int32_t t = parse_expr();
            if (t < 0)
                return -1;
            skip_ws();
            if (i_ >= n_ || p_[i_] != ':') {
                fail("expected ':'");
                return -1;
            }
            ++i_;
            skip_ws();
            const int32_t f = parse_expr();
            if (f < 0)
                return -1;
            return add_node(plural_node::cond, 0, cond, t, f);
        }
        return cond;
    }

    int32_t parse_or()
    {
        int32_t a = parse_and();
        if (a < 0)
            return -1;
        for (;;) {
            skip_ws();
            if (!peek2('|', '|'))
                break;
            i_ += 2;
            skip_ws();
            const int32_t b = parse_and();
            if (b < 0)
                return -1;
            a = add_node(plural_node::or_, 0, a, b, -1);
        }
        return a;
    }

    int32_t parse_and()
    {
        int32_t a = parse_eq();
        if (a < 0)
            return -1;
        for (;;) {
            skip_ws();
            if (!peek2('&', '&'))
                break;
            i_ += 2;
            skip_ws();
            const int32_t b = parse_eq();
            if (b < 0)
                return -1;
            a = add_node(plural_node::and_, 0, a, b, -1);
        }
        return a;
    }

    int32_t parse_eq()
    {
        int32_t a = parse_rel();
        if (a < 0)
            return -1;
        for (;;) {
            skip_ws();
            plural_node::op_t op = plural_node::num;
            if (peek2('=', '=')) { op = plural_node::eq; i_ += 2; }
            else if (peek2('!', '=')) { op = plural_node::ne; i_ += 2; }
            else break;
            skip_ws();
            const int32_t b = parse_rel();
            if (b < 0)
                return -1;
            a = add_node(op, 0, a, b, -1);
        }
        return a;
    }

    int32_t parse_rel()
    {
        int32_t a = parse_add();
        if (a < 0)
            return -1;
        for (;;) {
            skip_ws();
            plural_node::op_t op = plural_node::num;
            if (peek2('<', '=')) { op = plural_node::le; i_ += 2; }
            else if (peek2('>', '=')) { op = plural_node::ge; i_ += 2; }
            else if (i_ < n_ && p_[i_] == '<') { op = plural_node::lt; ++i_; }
            else if (i_ < n_ && p_[i_] == '>') { op = plural_node::gt; ++i_; }
            else break;
            skip_ws();
            const int32_t b = parse_add();
            if (b < 0)
                return -1;
            a = add_node(op, 0, a, b, -1);
        }
        return a;
    }

    int32_t parse_add()
    {
        int32_t a = parse_mul();
        if (a < 0)
            return -1;
        for (;;) {
            skip_ws();
            plural_node::op_t op = plural_node::num;
            if (i_ < n_ && p_[i_] == '+') { op = plural_node::add; ++i_; }
            else if (i_ < n_ && p_[i_] == '-') { op = plural_node::sub; ++i_; }
            else break;
            skip_ws();
            const int32_t b = parse_mul();
            if (b < 0)
                return -1;
            a = add_node(op, 0, a, b, -1);
        }
        return a;
    }

    int32_t parse_mul()
    {
        int32_t a = parse_un();
        if (a < 0)
            return -1;
        for (;;) {
            skip_ws();
            plural_node::op_t op = plural_node::num;
            if (i_ < n_ && p_[i_] == '*') { op = plural_node::mul; ++i_; }
            else if (i_ < n_ && p_[i_] == '/') { op = plural_node::div; ++i_; }
            else if (i_ < n_ && p_[i_] == '%') { op = plural_node::mod; ++i_; }
            else break;
            skip_ws();
            const int32_t b = parse_un();
            if (b < 0)
                return -1;
            a = add_node(op, 0, a, b, -1);
        }
        return a;
    }

    int32_t parse_un()
    {
        depth_guard g(depth_);
        if (!g.ok) {
            fail("expression too deep");
            return -1;
        }

        skip_ws();
        if (i_ < n_ && p_[i_] == '!') {
            ++i_;
            const int32_t a = parse_un();
            if (a < 0)
                return -1;
            return add_node(plural_node::not_, 0, a, -1, -1);
        }
        return parse_prim();
    }

    int32_t parse_prim()
    {
        skip_ws();
        if (i_ >= n_) {
            fail("unexpected end of expression");
            return -1;
        }

        const char c = p_[i_];
        if (c == 'n') {
            ++i_;
            return add_node(plural_node::var_n, 0, -1, -1, -1);
        }
        if (is_digit(c)) {
            uint64_t v = 0;
            if (!read_uint(v))
                return -1;
            return add_node(plural_node::num, v, -1, -1, -1);
        }
        if (c == '(') {
            ++i_;
            const int32_t inner = parse_expr();
            if (inner < 0)
                return -1;
            skip_ws();
            if (i_ >= n_ || p_[i_] != ')') {
                fail("expected ')'");
                return -1;
            }
            ++i_;
            return inner;
        }

        fail("unexpected character in expression");
        return -1;
    }

    const char* p_ = nullptr;
    size_t      n_ = 0;
    size_t      i_ = 0;
    uint32_t    depth_ = 0;
    uint32_t    nplurals_ = 2;
    int32_t     root_ = -1;
    std::vector<plural_node> nodes_;
    const char* err_ = "";
};

} // namespace details

// ---------------------------------------------------------------------------
// plural_family / plural_rule
// ---------------------------------------------------------------------------
//! @brief Plural form selection rule for one language.
//!
//! @details A rule maps a count `n` to a form index in [0, nplurals()). It is
//!          either one of the built-in GNU gettext families (evaluated by a
//!          switch, no interpreter) or a parsed expression program shared through
//!          a shared_ptr, so copying a rule is cheap.
//!
//! Usage:
//! @code
//!   // 1. Pick the rule automatically from a language tag.
//!   plural_rule r = plural_rule::for_language(make_tag("ru"));
//!   r.index(1);   // 0
//!   r.index(3);   // 1
//!   r.index(5);   // 2
//!
//!   // 2. Use a built-in family directly.
//!   plural_rule en = plural_rule::builtin(plural_family::en);
//!   en.index(1);  // 0, en.index(2) == 1
//!
//!   // 3. Parse a rule string from a .po header or a db column.
//!   plural_rule ru;
//!   memepp::string err;
//!   if (!plural_rule::parse("@ru", ru, &err)) {
//!       // err holds the reason, e.g. "unknown built-in family"
//!   }
//!
//!   // 4. Parse with an automatic fallback: use the text when it parses,
//!   //    otherwise derive the rule from the language.
//!   plural_rule r = plural_rule::from_text_or_language(rule_text, make_tag("pl"));
//! @endcode
//!
//! @note You normally do not call this class yourself: fast_catalog_builder and
//!       lite_catalog keep one rule per language internally.
class plural_rule
{
public:
    //! @brief Same as builtin(plural_family::en).
    plural_rule() noexcept
        : fam_(plural_family::en)
        , nplurals_(details::builtin_nplurals(plural_family::en))
    {}

    static plural_rule builtin(plural_family _f) noexcept
    {
        plural_rule r;
        if (_f == plural_family::expr)
            _f = plural_family::en;
        r.fam_ = _f;
        r.nplurals_ = details::builtin_nplurals(_f);
        r.prog_.reset();
        return r;
    }

    static plural_rule for_language(const tag_t& _tag) noexcept
    {
        if (!_tag.valid())
            return builtin(plural_family::en);

        const char* lang = reinterpret_cast<const char*>(_tag.language().data());
        const size_t len = _tag.language().size();

        auto is = [lang, len](const char* _w) noexcept {
            const size_t l = std::strlen(_w);
            return l == len && std::memcmp(lang, _w, l) == 0;
        };
        auto any = [&is](std::initializer_list<const char*> _ws) noexcept {
            for (const char* w : _ws) {
                if (is(w))
                    return true;
            }
            return false;
        };

        if (is("pt")) {
            const char* reg = reinterpret_cast<const char*>(_tag.region().data());
            const size_t rl = _tag.region().size();
            if (rl == 2 && std::memcmp(reg, "BR", 2) == 0)
                return builtin(plural_family::fr);
        }

        if (any({ "zh", "ja", "ko", "vi", "th", "id", "ms", "lo", "km", "my", "bo" }))
            return builtin(plural_family::one);
        if (any({ "fr", "oc", "ln", "fil", "ti" }))
            return builtin(plural_family::fr);
        if (any({ "ru", "uk", "be", "sr", "hr", "bs" }))
            return builtin(plural_family::ru);
        if (is("pl")) return builtin(plural_family::pl);
        if (any({ "cs", "sk" })) return builtin(plural_family::cs);
        if (is("lt")) return builtin(plural_family::lt);
        if (is("lv")) return builtin(plural_family::lv);
        if (is("ro")) return builtin(plural_family::ro);
        if (is("sl")) return builtin(plural_family::sl);
        if (is("ga")) return builtin(plural_family::ga);
        if (is("ar")) return builtin(plural_family::ar);

        return builtin(plural_family::en);
    }

    //! @brief Parse rule text:
    //!          ""                      -> false, err = "empty"
    //!          "@one" "@en" ... "@ar"  -> built-in family
    //!          "@xxx" (unknown)        -> false
    //!          anything else           -> gettext expression
    //!        On success writes _out and returns true; on failure _out is untouched.
    static bool parse(const memepp::string_view& _text, plural_rule& _out,
                      memepp::string* _err = nullptr)
    {
        auto fail = [&_err](const char* _m) -> bool {
            if (_err)
                *_err = memepp::string{ _m };
            return false;
        };

        if (_text.empty())
            return fail("empty");
        if (_text.size() > 1024)
            return fail("rule text too long");

        if (_text.at(0) == '@') {
            const size_t nl = static_cast<size_t>(_text.size()) - 1;
            const char* nm = _text.data() + 1;
            static const char* const names[] = {
                "one", "en", "fr", "ru", "pl", "cs", "lt", "lv", "ro", "sl", "ga", "ar"
            };
            for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
                if (std::strlen(names[i]) == nl && std::memcmp(nm, names[i], nl) == 0) {
                    _out = builtin(static_cast<plural_family>(i));
                    return true;
                }
            }
            return fail("unknown built-in family");
        }

        details::plural_compiler c(_text);
        if (!c.compile())
            return fail(c.error());

        plural_rule r;
        r.fam_ = plural_family::expr;
        r.nplurals_ = c.nplurals();
        std::shared_ptr<details::plural_program> prog = std::make_shared<details::plural_program>();
        prog->nodes = std::move(c.nodes());
        prog->root = c.root();
        prog->nplurals = c.nplurals();
        r.prog_ = std::move(prog);

        _out = std::move(r);
        return true;
    }

    //! @brief Use the parsed text when it succeeds, otherwise the language default.
    static plural_rule from_text_or_language(const memepp::string_view& _text,
                                             const tag_t& _tag) noexcept
    {
        plural_rule r;
        try {
            if (parse(_text, r))
                return r;
        } catch (...) {
        }
        return for_language(_tag);
    }

    plural_family family()   const noexcept { return fam_; }
    uint32_t      nplurals() const noexcept { return nplurals_; }

    //! @brief Result is guaranteed to be < nplurals().
    uint32_t index(uint64_t _n) const noexcept
    {
        uint32_t i = 0;
        if (prog_)
            i = static_cast<uint32_t>(prog_->eval(prog_->root, _n));
        else
            i = details::builtin_index(fam_, _n);

        if (nplurals_ != 0 && i >= nplurals_)
            i = nplurals_ - 1;
        return i;
    }

private:
    plural_family fam_ = plural_family::en;
    uint32_t      nplurals_ = 2;
    std::shared_ptr<const details::plural_program> prog_;
};

// ---------------------------------------------------------------------------
// Abstract base class
// ---------------------------------------------------------------------------
//! @brief Common interface of every catalog: the four translation calls, the
//!        language settings and the fallback-chain lookup that derived classes
//!        implement through lookup_one().
//!
//! @details Do not instantiate this class directly; use fast_catalog (built by
//!          fast_catalog_builder) or lite_catalog (opened from a sqlite file).
//!          The reference returned by tr*() is owned by the caller; on a miss the
//!          original text (msgid / msgid_plural) is returned instead.
//!
//! Usage:
//! @code
//!   // --- Obtain some catalog (fast or lite) ---
//!   fast_catalog_builder b;
//!   b.set_source_language(make_tag("en"));
//!   b.add(make_tag("zh-CN"), "", "Open", "Open in Chinese");
//!   std::shared_ptr<fast_catalog> cat = b.build();
//!
//!   // --- Four translation flavours ---
//!   cat->tr(make_tag("zh-CN"), "Open");                        // plain
//!   cat->tr_ctx(make_tag("zh-CN"), "menu", "Open");            // with context
//!   cat->tr_n(make_tag("ru"), "%d file", "%d files", 5);       // plural
//!   cat->tr_ctx_n(make_tag("ru"), "dlg", "%d f", "%d fs", 5);  // plural + context
//!
//!   // --- Tag-less overloads use this catalog's default language ---
//!   cat->set_default_language(make_tag("zh-CN"));
//!   cat->tr("Open");
//!
//!   // --- Source language: translations for it are never looked up ---
//!   cat->set_source_language(make_tag("en"));  // default
//!
//!   // --- Direct single-language lookup (no fallback chain) ---
//!   memepp::string out;
//!   if (cat->lookup(make_tag("zh-CN"), "", "Open", false, 0, out)) {
//!       // hit; out holds the translation
//!   }
//! @endcode
//!
//! @warning Never pass a string literal where a tag_t is expected: the deleted
//!          overloads turn that into a compile error on purpose, because the
//!          implicit simple_tag(string_view) constructor would silently build a
//!          truncated, invalid tag. Write make_tag("zh-CN") instead.
class catalog
{
public:
    virtual ~catalog() = default;
    catalog(const catalog&) = delete;
    catalog& operator=(const catalog&) = delete;

    // ---- language settings (thread safe) ----
    void set_default_language(const tag_t& _tag)
    {
        default_.store(_tag);
    }

    tag_t default_language() const
    {
        return default_.load();
    }

    void set_source_language(const tag_t& _tag)
    {
        source_.store(_tag);
    }

    tag_t source_language() const
    {
        return source_.load();
    }

    // ---- plain text ----
    memepp::string tr(const memepp::string_view& _msgid) const
    {
        return tr(default_language(), _msgid);
    }

    memepp::string tr(const tag_t& _tag, const memepp::string_view& _msgid) const
    {
        memepp::string out;
        if (lookup(_tag, memepp::string_view{}, _msgid, false, 0, out))
            return out;
        return _msgid.to_string();
    }

    // ---- with context ----
    memepp::string tr_ctx(const memepp::string_view& _ctx,
                          const memepp::string_view& _msgid) const
    {
        return tr_ctx(default_language(), _ctx, _msgid);
    }

    memepp::string tr_ctx(const tag_t& _tag, const memepp::string_view& _ctx,
                          const memepp::string_view& _msgid) const
    {
        memepp::string out;
        if (lookup(_tag, _ctx, _msgid, false, 0, out))
            return out;
        return _msgid.to_string();
    }

    // ---- plural ----
    memepp::string tr_n(const memepp::string_view& _msgid,
                        const memepp::string_view& _msgid_plural, uint64_t _n) const
    {
        return tr_n(default_language(), _msgid, _msgid_plural, _n);
    }

    memepp::string tr_n(const tag_t& _tag, const memepp::string_view& _msgid,
                        const memepp::string_view& _msgid_plural, uint64_t _n) const
    {
        return tr_ctx_n(_tag, memepp::string_view{}, _msgid, _msgid_plural, _n);
    }

    // ---- plural with context ----
    memepp::string tr_ctx_n(const memepp::string_view& _ctx, const memepp::string_view& _msgid,
                            const memepp::string_view& _msgid_plural, uint64_t _n) const
    {
        return tr_ctx_n(default_language(), _ctx, _msgid, _msgid_plural, _n);
    }

    memepp::string tr_ctx_n(const tag_t& _tag, const memepp::string_view& _ctx,
                            const memepp::string_view& _msgid,
                            const memepp::string_view& _msgid_plural, uint64_t _n) const
    {
        memepp::string out;
        if (lookup(_tag, _ctx, _msgid, true, _n, out))
            return out;
        return _n == 1 ? _msgid.to_string() : _msgid_plural.to_string();
    }

    // ---- misuse guard: a string where a tag is expected must not compile ----
    memepp::string tr(const memepp::string_view&, const memepp::string_view&) const = delete;
    memepp::string tr_ctx(const memepp::string_view&, const memepp::string_view&,
                          const memepp::string_view&) const = delete;
    memepp::string tr_n(const memepp::string_view&, const memepp::string_view&,
                        const memepp::string_view&, uint64_t) const = delete;
    memepp::string tr_ctx_n(const memepp::string_view&, const memepp::string_view&,
                            const memepp::string_view&, const memepp::string_view&,
                            uint64_t) const = delete;

    //! @brief Walk the fallback chain (design section 4.2). Returns true and writes
    //!        _out on a hit; returns false and leaves _out untouched on a miss.
    //!        When _plural is false, _n is ignored and form 0 is used.
    bool lookup(const tag_t& _tag, const memepp::string_view& _ctx,
                const memepp::string_view& _msgid, bool _plural, uint64_t _n,
                memepp::string& _out) const
    {
        try {
            const details::tag_chain chain = details::make_chain(_tag);
            const tag_t src = source_.load();
            for (int i = 0; i < chain.n; ++i) {
                if (chain.v[i] == src)
                    return false;   // the source language has no translation of its own
                if (lookup_one(chain.v[i], _ctx, _msgid, _plural, _n, _out))
                    return true;
            }
        } catch (...) {
        }
        return false;
    }

protected:
    catalog()
        : source_(make_tag("en"))
        , default_(make_tag("en"))
    {}

    //! @brief Snapshot of the source language; derived classes need it for their
    //!        own fallback-chain walks. A value copy (32 bytes), no heap traffic.
    tag_t source_language_value() const
    {
        return source_.load();
    }

    //! @brief Look up a single language (no fallback). Plural form selection
    //!        (rule + out-of-range truncation) is done by the derived class.
    virtual bool lookup_one(const tag_t& _lang, const memepp::string_view& _ctx,
                            const memepp::string_view& _msgid, bool _plural, uint64_t _n,
                            memepp::string& _out) const = 0;

private:
    // A locale tag is trivially copyable and only 32 bytes, so a seqlock is a
    // far cheaper way to publish it than a reference-counted shared_ptr: readers
    // take no lock and allocate nothing.
    mgpp::thrd::seqlock<tag_t> source_;
    mgpp::thrd::seqlock<tag_t> default_;
};

// ---------------------------------------------------------------------------
// fast mode: internal storage
// ---------------------------------------------------------------------------
namespace details {

//! @brief Reference-counted contiguous pool holding all ctx / msgid / msgstr
//!        bytes of one language, each segment followed by '\0'.
//!
//! @details Lifetime is managed by std::shared_ptr. Instances are created only
//!          through create(): the pass-key idiom keeps the constructor out of
//!          reach while still allowing std::make_shared to allocate.
class pool_block
{
public:
    //! @brief Pass key: only pool_block itself can build one, so only create()
    //!        can construct a pool_block.
    struct ctor_key
    {
    private:
        ctor_key() = default;
        friend class pool_block;
    };

    static std::shared_ptr<pool_block> create(std::vector<char>&& _bytes)
    {
        return std::make_shared<pool_block>(ctor_key{}, std::move(_bytes));
    }

    explicit pool_block(ctor_key, std::vector<char>&& _b)
        : bytes_(std::move(_b))
    {
        live_pool_blocks().fetch_add(1);
    }

    pool_block(const pool_block&) = delete;
    pool_block& operator=(const pool_block&) = delete;

    ~pool_block()
    {
        live_pool_blocks().fetch_sub(1);
    }

    const char* data() const noexcept { return bytes_.data(); }

private:
    std::vector<char> bytes_;
};

struct form_rec
{
    uint32_t off;
    uint32_t len;   //!< 0xFFFFFFFF means "absent"
};

struct entry_rec
{
    uint64_t hash;
    uint32_t ctx_off, ctx_len;
    uint32_t id_off, id_len;
    uint32_t form_begin, form_count;
};

//! @brief Immutable per-language table: open-addressing flat index over entries.
struct lang_table
{
    static constexpr uint32_t k_absent = 0xFFFFFFFFu;

    tag_t                       tag;
    plural_rule                 rule;
    std::shared_ptr<pool_block> blk;
    std::vector<entry_rec>      entries;
    std::vector<form_rec>       forms;
    std::vector<uint32_t>       slots;

    lang_table() = default;
    lang_table(const lang_table&) = delete;
    lang_table& operator=(const lang_table&) = delete;

    lang_table(lang_table&& _o) noexcept = default;
    lang_table& operator=(lang_table&& _o) noexcept = default;
    ~lang_table() = default;

    const entry_rec* find(const memepp::string_view& _ctx,
                          const memepp::string_view& _id) const noexcept
    {
        if (slots.empty() || !blk)
            return nullptr;

        const uint64_t h = key_hash(_ctx, _id);
        const uint32_t cap = static_cast<uint32_t>(slots.size());
        uint32_t i = static_cast<uint32_t>(h) & (cap - 1);

        for (;;) {
            const uint32_t s = slots[i];
            if (s == k_absent)
                return nullptr;

            const entry_rec& e = entries[s];
            if (e.hash == h
                && e.ctx_len == static_cast<uint32_t>(_ctx.size())
                && e.id_len == static_cast<uint32_t>(_id.size())) {
                const char* base = blk->data();
                bool same = true;
                if (e.ctx_len != 0 && std::memcmp(base + e.ctx_off, _ctx.data(), e.ctx_len) != 0)
                    same = false;
                if (same && e.id_len != 0
                    && std::memcmp(base + e.id_off, _id.data(), e.id_len) != 0)
                    same = false;
                if (same)
                    return &e;
            }
            i = (i + 1) & (cap - 1);
        }
    }
};

} // namespace details

// ---------------------------------------------------------------------------
// fast_catalog
// ---------------------------------------------------------------------------
//! @brief Immutable, fully in-memory catalog. Built once by
//!        fast_catalog_builder; after build() the data never changes.
//!
//! @details Every language owns one contiguous byte pool plus a flat
//!          open-addressing index, so a lookup touches only two or three cache
//!          lines and takes no lock. Long translations are returned zero-copy
//!          through MemeString's takeover mechanism, which keeps the pool block
//!          alive until the last copy of the string dies.
//!
//! Usage:
//! @code
//!   // --- Build it (startup only, single thread) ---
//!   fast_catalog_builder b;
//!   b.set_source_language(make_tag("en"));
//!   b.add(make_tag("zh-CN"), "", "Open", "Open in Chinese");
//!   b.add_plural(make_tag("ru"), "", "%d file",
//!                { "%d file-0", "%d file-1", "%d file-2" });
//!   std::shared_ptr<fast_catalog> cat = b.build();
//!
//!   // --- Query it (thread-safe, lock-free) ---
//!   cat->tr(make_tag("zh-CN"), "Open");
//!   cat->tr_n(make_tag("ru"), "%d file", "%d files", 21);
//!
//!   // --- Introspection ---
//!   cat->language_count();                  // number of languages
//!   cat->has_language(make_tag("zh-CN"));   // true
//!   cat->message_count(make_tag("zh-CN"));  // plural entries count as one
//!
//!   // --- Zero-copy view, host-internal only ---
//!   memepp::string_view v;
//!   if (cat->find_view(make_tag("zh-CN"), "", "Open", false, 0, v)) {
//!       // v points into the pool; valid while `cat` is alive. No allocation.
//!   }
//! @endcode
//!
//! @warning Strings returned by tr*() may outlive the catalog, but a
//!          memepp::string_view from find_view() does not: keep the catalog alive
//!          (or copy the view) for as long as the view is used.
class fast_catalog final : public catalog
{
public:
    //! @brief Zero-copy query, for host-internal use only. The view stays valid as
    //!        long as this object is alive. Same semantics as lookup().
    bool find_view(const tag_t& _tag, const memepp::string_view& _ctx,
                   const memepp::string_view& _msgid, bool _plural, uint64_t _n,
                   memepp::string_view& _out) const
    {
        try {
            const details::tag_chain chain = details::make_chain(_tag);
            const tag_t src = source_language_value();
            for (int i = 0; i < chain.n; ++i) {
                if (chain.v[i] == src)
                    return false;
                std::shared_ptr<details::pool_block> blk;
                details::form_rec f{ 0, 0 };
                if (!find_form(chain.v[i], _ctx, _msgid, _plural, _n, blk, f))
                    continue;
                _out = memepp::string_view{
                    blk->data() + f.off,
                    static_cast<memepp::string_view::size_type>(f.len) };
                return true;
            }
        } catch (...) {
        }
        return false;
    }

    size_t language_count() const noexcept { return tables_.size(); }

    bool has_language(const tag_t& _tag) const noexcept
    {
        return find_table(_tag) != nullptr;
    }

    //! @brief Number of entries of that language (a plural entry counts as one).
    size_t message_count(const tag_t& _tag) const noexcept
    {
        const details::lang_table* t = find_table(_tag);
        return t ? t->entries.size() : 0;
    }

protected:
    bool lookup_one(const tag_t& _lang, const memepp::string_view& _ctx,
                    const memepp::string_view& _msgid, bool _plural, uint64_t _n,
                    memepp::string& _out) const override
    {
        std::shared_ptr<details::pool_block> blk;
        details::form_rec f{ 0, 0 };
        if (!find_form(_lang, _ctx, _msgid, _plural, _n, blk, f))
            return false;
        _out = make_result(blk, f);
        return true;
    }

private:
    friend class fast_catalog_builder;

public:
    //! @brief Construction key: only fast_catalog_builder can create one.
    struct ctor_key
    {
    private:
        ctor_key() = default;
        friend class fast_catalog_builder;
    };

    explicit fast_catalog(ctor_key) {}

private:
    const details::lang_table* find_table(const tag_t& _tag) const noexcept
    {
        for (const details::lang_table& t : tables_) {
            if (t.tag == _tag)
                return &t;
        }
        return nullptr;
    }

    //! @brief Locate the (lang, ctx, msgid) form, resolving the plural index and
    //!        the "hole" rules (design section 5.2).
    bool find_form(const tag_t& _lang, const memepp::string_view& _ctx,
                   const memepp::string_view& _msgid, bool _plural, uint64_t _n,
                   std::shared_ptr<details::pool_block>& _blk, details::form_rec& _f) const
    {
        const details::lang_table* t = find_table(_lang);
        if (!t)
            return false;

        const details::entry_rec* e = t->find(_ctx, _msgid);
        if (!e)
            return false;

        uint32_t idx = 0;
        if (_plural) {
            idx = t->rule.index(_n);
            if (idx >= e->form_count)
                idx = e->form_count - 1;
        }

        const details::form_rec& f = t->forms[e->form_begin + idx];
        if (f.len == details::lang_table::k_absent)
            return false;

        _blk = t->blk;
        _f = f;
        return true;
    }

    //! @brief Build the returned string; long translations are taken over
    //!        (zero copy) with the pool block reference kept alive.
    static memepp::string make_result(const std::shared_ptr<details::pool_block>& _blk, const details::form_rec& _f)
    {
        if (_f.len == 0 || !_blk)
            return memepp::string{};

        // Capture a shared_ptr to keep the pool alive as long as the string exists
        return memepp::convert::takeover(
            const_cast<char*>(_blk->data() + _f.off),
            static_cast<mmint_t>(_f.len),
            [_blk](void*) noexcept { /* shared_ptr destructor handles cleanup */ });
    }

    std::vector<details::lang_table> tables_;
};

// ---------------------------------------------------------------------------
// fast_catalog_builder
// ---------------------------------------------------------------------------
//! @brief Single-threaded builder for fast_catalog.
//!
//! @details Accumulates translations in memory, then produces an immutable
//!          fast_catalog via build(). After build() the builder is empty and may
//!          be reused to build another catalog.
//!
//! Usage:
//! @code
//!   fast_catalog_builder b;
//!   b.set_source_language(make_tag("en"));
//!   b.set_default_language(make_tag("zh-CN"));
//!
//!   // Plain entries
//!   b.add(make_tag("zh-CN"), "", "Open", "Open in Chinese");
//!   b.add(make_tag("zh-CN"), "menu", "File", "File menu");
//!
//!   // Plural entries (forms are indexed 0..n-1)
//!   b.add_plural(make_tag("ru"), "", "%d file",
//!                { "%d file-0", "%d file-1", "%d file-2" });
//!
//!   // Optional: override the plural rule for a language
//!   b.set_plural_rule(make_tag("ru"), "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : ...;");
//!
//!   // Optional: load from a SQLite database (same schema as lite_catalog)
//!   mgpp::err e = b.load_from_sqlite("path/to/translations.db");
//!
//!   // Build the immutable catalog
//!   std::shared_ptr<fast_catalog> cat = b.build();
//! @endcode
//!
//! @warning Not thread-safe. Build on one thread, then share the resulting
//!          fast_catalog across threads.
class fast_catalog_builder
{
public:
    fast_catalog_builder() = default;
    fast_catalog_builder(const fast_catalog_builder&) = delete;
    fast_catalog_builder& operator=(const fast_catalog_builder&) = delete;
    fast_catalog_builder(fast_catalog_builder&&) noexcept = default;
    fast_catalog_builder& operator=(fast_catalog_builder&&) noexcept = default;
    ~fast_catalog_builder() = default;

    fast_catalog_builder& set_source_language(const tag_t& _tag)
    {
        src_ = _tag;
        return *this;
    }

    fast_catalog_builder& set_default_language(const tag_t& _tag)
    {
        def_ = _tag;
        return *this;
    }

    //! @brief Declare a language and give it a rule text (may be omitted; an
    //!        omitted or unparsable text falls back to for_language()).
    fast_catalog_builder& set_plural_rule(const tag_t& _lang,
                                          const memepp::string_view& _rule_text)
    {
        if (!_lang.valid())
            return *this;
        pending_lang& pl = lang_slot(_lang);
        pl.rule_text = _rule_text.to_string();
        return *this;
    }

    //! @brief Plain / contextual entry ("" ctx means no context). Duplicate keys:
    //!        the later one wins. An invalid _lang is ignored.
    fast_catalog_builder& add(const tag_t& _lang, const memepp::string_view& _ctx,
                              const memepp::string_view& _msgid,
                              const memepp::string_view& _msgstr)
    {
        if (!_lang.valid())
            return *this;
        std::vector<memepp::string> forms(1, _msgstr.to_string());
        std::vector<bool> present(1, true);
        put_forms(_lang, _ctx, _msgid, std::move(forms), std::move(present));
        return *this;
    }

    //! @brief Plural entry; _forms[i] is form i. Empty _forms is ignored.
    fast_catalog_builder& add_plural(const tag_t& _lang, const memepp::string_view& _ctx,
                                     const memepp::string_view& _msgid,
                                     std::initializer_list<memepp::string_view> _forms)
    {
        if (!_lang.valid() || _forms.size() == 0)
            return *this;
        std::vector<memepp::string> forms;
        forms.reserve(_forms.size());
        for (const memepp::string_view& f : _forms)
            forms.push_back(f.to_string());
        std::vector<bool> present(forms.size(), true);
        put_forms(_lang, _ctx, _msgid, std::move(forms), std::move(present));
        return *this;
    }

    fast_catalog_builder& add_plural(const tag_t& _lang, const memepp::string_view& _ctx,
                                     const memepp::string_view& _msgid,
                                     const std::vector<memepp::string>& _forms)
    {
        if (!_lang.valid() || _forms.empty())
            return *this;
        std::vector<memepp::string> forms(_forms);
        std::vector<bool> present(forms.size(), true);
        put_forms(_lang, _ctx, _msgid, std::move(forms), std::move(present));
        return *this;
    }

#if MMBKPP_I18N_ENABLE_SQLITE
    //! @brief Read a whole db (i18n_language + i18n_message) into the builder.
    //!        Missing form positions keep the "hole" information so that the
    //!        result is bit-identical with lite mode. Rows whose lang is not in
    //!        i18n_language, and rows with form < 0 or form >= 16, are ignored.
    //!        May be called more than once; later entries win.
    mgpp::err load_from_sqlite(const char* _path_utf8);
#endif

    //! @brief Produce the immutable catalog. The builder is cleared afterwards.
    std::shared_ptr<fast_catalog> build()
    {
        std::shared_ptr<fast_catalog> cat = std::make_shared<fast_catalog>(fast_catalog::ctor_key{});
        cat->set_source_language(src_);
        cat->set_default_language(def_);
        cat->tables_.reserve(langs_.size());

        for (pending_lang& pl : langs_) {
            details::lang_table t;
            t.tag = pl.tag;
            t.rule = plural_rule::from_text_or_language(pl.rule_text, pl.tag);

            std::vector<char> bytes;
            t.entries.reserve(pl.entries.size());
            t.forms.reserve(pl.entries.size() * 2);

            auto append_seg = [&bytes](const memepp::string& _s, uint32_t& _len) -> uint32_t {
                const size_t n = static_cast<size_t>(_s.size());
                // Check 4GiB limit before appending
                if (bytes.size() + n + 1 > 0xFFFFFFFFull)
                    throw std::length_error("i18n: language string pool exceeds 4GiB");
                const uint32_t off = static_cast<uint32_t>(bytes.size());
                const char* p = _s.data();
                bytes.insert(bytes.end(), p, p + n);
                bytes.push_back('\0');
                _len = static_cast<uint32_t>(n);
                return off;
            };

            for (pending_entry& e : pl.entries) {
                details::entry_rec er{};
                er.ctx_off = append_seg(e.ctx, er.ctx_len);
                er.id_off = append_seg(e.id, er.id_len);
                er.hash = details::key_hash(memepp::string_view{ e.ctx },
                                            memepp::string_view{ e.id });
                er.form_begin = static_cast<uint32_t>(t.forms.size());
                er.form_count = static_cast<uint32_t>(e.forms.size());

                // Copy the "nearest present form to the left" into every hole;
                // a leading run of holes stays absent.
                for (size_t i = 0; i < e.forms.size(); ++i) {
                    details::form_rec f{ 0, details::lang_table::k_absent };
                    if (e.present[i]) {
                        f.off = append_seg(e.forms[i], f.len);
                    } else if (i > 0) {
                        const details::form_rec prev = t.forms[er.form_begin + i - 1];
                        if (prev.len != details::lang_table::k_absent)
                            f = prev;
                    }
                    t.forms.push_back(f);
                }

                t.entries.push_back(er);
            }

            // Open-addressing flat index: power-of-two capacity, load factor <= 0.7.
            const size_t count = t.entries.size();
            size_t cap = 8;
            while (cap * 7 < count * 10)
                cap <<= 1;
            t.slots.assign(cap, details::lang_table::k_absent);
            for (uint32_t k = 0; k < static_cast<uint32_t>(count); ++k) {
                const uint32_t mask = static_cast<uint32_t>(cap - 1);
                uint32_t i = static_cast<uint32_t>(t.entries[k].hash) & mask;
                while (t.slots[i] != details::lang_table::k_absent)
                    i = (i + 1) & mask;
                t.slots[i] = k;
            }

            if (bytes.empty())
                bytes.push_back('\0');   // keep data() valid for empty tables
            t.blk = details::pool_block::create(std::move(bytes));

            cat->tables_.push_back(std::move(t));
        }

        langs_.clear();
        return cat;
    }

private:
    struct pending_entry
    {
        memepp::string              ctx;
        memepp::string              id;
        std::vector<memepp::string> forms;
        std::vector<bool>           present;
    };

    struct pending_lang
    {
        tag_t                                   tag;
        memepp::string                          rule_text;
        std::unordered_map<std::string, size_t> index;   //!< "ctx\x04id" -> entry index
        std::vector<pending_entry>              entries;
    };

    pending_lang& lang_slot(const tag_t& _lang)
    {
        for (pending_lang& pl : langs_) {
            if (pl.tag == _lang)
                return pl;
        }
        langs_.push_back(pending_lang{});
        langs_.back().tag = _lang;
        return langs_.back();
    }

    static std::string make_key(const memepp::string_view& _ctx,
                                const memepp::string_view& _id)
    {
        std::string k;
        k.reserve(static_cast<size_t>(_ctx.size()) + static_cast<size_t>(_id.size()) + 1);
        if (_ctx.size() > 0)
            k.append(_ctx.data(), static_cast<size_t>(_ctx.size()));
        k.push_back('\x04');
        if (_id.size() > 0)
            k.append(_id.data(), static_cast<size_t>(_id.size()));
        return k;
    }

    void put_forms(const tag_t& _lang, const memepp::string_view& _ctx,
                   const memepp::string_view& _msgid,
                   std::vector<memepp::string>&& _forms, std::vector<bool>&& _present)
    {
        pending_lang& pl = lang_slot(_lang);
        const std::string k = make_key(_ctx, _msgid);
        auto it = pl.index.find(k);
        if (it != pl.index.end()) {
            pending_entry& e = pl.entries[it->second];
            e.forms = std::move(_forms);
            e.present = std::move(_present);
            return;
        }

        pending_entry e;
        e.ctx = _ctx.to_string();
        e.id = _msgid.to_string();
        e.forms = std::move(_forms);
        e.present = std::move(_present);
        pl.index.emplace(k, pl.entries.size());
        pl.entries.push_back(std::move(e));
    }

    std::vector<pending_lang> langs_;
    tag_t src_ = make_tag("en");
    tag_t def_ = make_tag("en");
};

#if MMBKPP_I18N_ENABLE_SQLITE

// ---------------------------------------------------------------------------
// lite mode
// ---------------------------------------------------------------------------
//! @brief Configuration options for lite_catalog.
//!
//! @details Controls SQLite connection behavior and optional per-thread LRU cache.
//!
//! Usage:
//! @code
//!   lite_options opts;
//!   opts.cache_capacity = 1000;       // Enable LRU cache with 1000 entries per thread
//!   opts.sqlite_cache_kb = 256;       // Increase SQLite page cache to 256 KB
//!   opts.mmap = true;                 // Enable memory-mapped I/O
//!   opts.busy_timeout_ms = 5000;      // Wait up to 5 seconds for locks
//!
//!   auto cat = lite_catalog::open("translations.db", opts);
//! @endcode
struct lite_options
{
    size_t cache_capacity  = 0;      //!< LRU entries per thread; 0 = off
    int    sqlite_cache_kb = 64;     //!< PRAGMA cache_size = -sqlite_cache_kb
    bool   mmap            = false;  //!< false -> PRAGMA mmap_size = 0
    int    busy_timeout_ms = 1000;   //!< sqlite3_busy_timeout
};

namespace details {

struct sqlite_db_closer
{
    void operator()(sqlite3* _p) const noexcept
    {
        if (_p)
            sqlite3_close(_p);
    }
};

using sqlite_db_ptr = std::unique_ptr<sqlite3, sqlite_db_closer>;

//! @brief Classic list + unordered_map LRU. One instance per connection (thread),
//!        so no locking is needed.
struct lru_cache
{
    struct val_t
    {
        bool           found = false;
        memepp::string text;
    };

    using list_t = std::list<std::pair<std::string, val_t>>;

    size_t cap = 0;
    list_t lst;                                          //!< front = most recent
    std::unordered_map<std::string, list_t::iterator> idx;

    bool   enabled() const noexcept { return cap != 0; }
    size_t size()    const noexcept { return lst.size(); }

    const val_t* get(const std::string& _k)
    {
        auto it = idx.find(_k);
        if (it == idx.end())
            return nullptr;
        lst.splice(lst.begin(), lst, it->second);
        return &it->second->second;
    }

    void put(std::string _k, bool _found, const memepp::string& _text)
    {
        if (cap == 0)
            return;

        auto it = idx.find(_k);
        if (it != idx.end()) {
            it->second->second.found = _found;
            it->second->second.text = _text;
            lst.splice(lst.begin(), lst, it->second);
            return;
        }

        lst.emplace_front(_k, val_t{ _found, _text });
        idx.emplace(std::move(_k), lst.begin());
        while (lst.size() > cap) {
            auto last = std::prev(lst.end());
            idx.erase(last->first);
            lst.pop_back();
        }
    }
};

struct conn
{
    sqlite3*      db = nullptr;
    sqlite3_stmt* stmt = nullptr;
    lru_cache     cache;

    ~conn()
    {
        if (stmt)
            sqlite3_finalize(stmt);
        if (db)
            sqlite3_close(db);
    }
};

//! @brief Owns every connection opened for one lite_catalog.
struct conn_pool
{
    static const char* select_sql()
    {
        return "SELECT msgstr FROM i18n_message"
               " WHERE lang=?1 AND ctx=?2 AND msgid=?3 AND form<=?4"
               " ORDER BY form DESC LIMIT 1";
    }

    std::string                         path;   //!< UTF-8
    lite_options                        opts;
    mutable std::mutex                  mtx;
    std::vector<std::unique_ptr<conn>>  conns;

    conn_pool(std::string _path, const lite_options& _opts)
        : path(std::move(_path))
        , opts(_opts)
    {}

    conn* open_conn()
    {
        sqlite3* db = nullptr;
        int rc = sqlite3_open_v2(path.c_str(), &db,
                                 SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr);
        if (rc != SQLITE_OK) {
            if (db)
                sqlite3_close(db);
            return nullptr;
        }

        sqlite3_busy_timeout(db, opts.busy_timeout_ms);

        const std::string cache_pragma =
            "PRAGMA cache_size=-" + std::to_string(opts.sqlite_cache_kb);
        sqlite3_exec(db, cache_pragma.c_str(), nullptr, nullptr, nullptr);
        if (!opts.mmap)
            sqlite3_exec(db, "PRAGMA mmap_size=0", nullptr, nullptr, nullptr);

        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db, select_sql(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            if (stmt)
                sqlite3_finalize(stmt);
            sqlite3_close(db);
            return nullptr;
        }

        auto c = std::make_unique<conn>();
        c->db = db;
        c->stmt = stmt;
        c->cache.cap = opts.cache_capacity;

        conn* raw = c.get();
        {
            std::lock_guard<std::mutex> lk(mtx);
            conns.push_back(std::move(c));
        }
        return raw;
    }

    //! @brief Called on thread exit. Deletes the connection only when it is still
    //!        registered, so a second call is harmless.
    void close_conn(conn* _c)
    {
        std::lock_guard<std::mutex> lk(mtx);
        auto it = std::find_if(conns.begin(), conns.end(),
            [_c](const std::unique_ptr<conn>& p) { return p.get() == _c; });
        if (it != conns.end()) {
            conns.erase(it);  // unique_ptr destructor cleans up
        }
    }

    size_t count() const
    {
        std::lock_guard<std::mutex> lk(mtx);
        return conns.size();
    }
};

//! @brief Per-thread registration of the connections this thread opened.
//!        The destructor runs at thread exit and closes them.
struct tls_conns
{
    struct item
    {
        std::weak_ptr<conn_pool> owner;
        conn*                    c = nullptr;
    };

    std::vector<item> items;

    ~tls_conns()
    {
        for (item& it : items) {
            if (std::shared_ptr<conn_pool> p = it.owner.lock())
                p->close_conn(it.c);
        }
    }

    static tls_conns& get()
    {
        thread_local tls_conns t;
        return t;
    }

    //! @brief Drop this thread's entry for _pool without closing the connection.
    //!        Returns the connection so the caller can close it.
    conn* unregister(const std::shared_ptr<conn_pool>& _pool) noexcept
    {
        for (size_t i = 0; i < items.size();) {
            item& it = items[i];
            if (it.owner.expired()) {
                items.erase(items.begin() + static_cast<ptrdiff_t>(i));
                continue;
            }
            if (!it.owner.owner_before(_pool) && !_pool.owner_before(it.owner)) {
                conn* c = it.c;
                items.erase(items.begin() + static_cast<ptrdiff_t>(i));
                return c;
            }
            ++i;
        }
        return nullptr;
    }

    conn* find(const std::shared_ptr<conn_pool>& _pool) noexcept
    {
        for (size_t i = 0; i < items.size();) {
            item& it = items[i];
            if (it.owner.expired()) {
                items.erase(items.begin() + static_cast<ptrdiff_t>(i));
                continue;
            }
            if (!it.owner.owner_before(_pool) && !_pool.owner_before(it.owner))
                return it.c;
            ++i;
        }
        return nullptr;
    }
};

//! @brief Get (opening on first use) this thread's connection to _pool.
inline conn* acquire(const std::shared_ptr<conn_pool>& _pool)
{
    if (!_pool)
        return nullptr;

    tls_conns& t = tls_conns::get();
    if (conn* c = t.find(_pool))
        return c;

    conn* c = _pool->open_conn();
    if (c)
        t.items.push_back(tls_conns::item{ _pool, c });
    return c;
}

} // namespace details

// ---------------------------------------------------------------------------
// lite_catalog
// ---------------------------------------------------------------------------
//! @brief Low-footprint catalog that queries a SQLite database directly instead
//!        of loading the translations into memory.
//!
//! @details open() reads only the i18n_language table (language tags and plural
//!          rules); message rows are fetched on demand. Each thread gets its own
//!          read-only connection, opened lazily on first use and closed when the
//!          thread exits. An optional per-thread LRU cache can be enabled through
//!          lite_options::cache_capacity.
//!
//! Usage:
//! @code
//!   // --- Open the database (no message row is read here) ---
//!   lite_options opts;
//!   opts.cache_capacity = 512;   // 0 disables the cache (default)
//!   mgpp::err e;
//!   std::shared_ptr<lite_catalog> cat =
//!       lite_catalog::open("plugin_x.db", opts, &e);
//!   if (!cat) {
//!       // e.message() explains why (missing file, missing table, ...)
//!   }
//!
//!   // --- Query it (thread-safe; opens one connection per thread) ---
//!   cat->tr(make_tag("zh-CN"), "Open");
//!   cat->tr_n(make_tag("ru"), "%d file", "%d files", 21);
//!
//!   // --- Introspection and per-thread connection control ---
//!   cat->connection_count();          // live connections for this instance
//!   cat->thread_cache_size();         // LRU entries of the calling thread
//!   cat->release_thread_connection(); // close this thread's connection early
//!
//!   // --- Unload a plugin safely ---
//!   auto tk = registry.remove("plugin_x");
//!   cat.reset();                       // drop your own reference too
//!   tk.wait_released(std::chrono::milliseconds(1000));
//!   // now every connection is closed and the db file can be deleted
//! @endcode
//!
//! @warning The destructor must not run while another thread is still calling
//!          into this instance. Use catalog_registry::remove() +
//!          release_token::wait_released() to be sure, then unload the DLL or
//!          delete the database file.
class lite_catalog final : public catalog
{
public:
    ~lite_catalog() override = default;

    //! @brief Returns nullptr on failure; a non-null _err receives the reason.
    static std::shared_ptr<lite_catalog> open(const char* _path_utf8,
                                              const lite_options& _opts = {},
                                              mgpp::err* _err = nullptr)
    {
        if (!_path_utf8 || !*_path_utf8) {
            if (_err)
                *_err = mgpp::err{ MGEC__INVAL, memepp::string{ "empty sqlite path" } };
            return nullptr;
        }

        sqlite3* raw = nullptr;
        int rc = sqlite3_open_v2(_path_utf8, &raw, SQLITE_OPEN_READONLY, nullptr);
        if (rc != SQLITE_OK) {
            const int32_t code = mgec__from_sqlite3_err(rc);
            const memepp::string msg{ raw ? sqlite3_errmsg(raw) : "cannot open sqlite file" };
            if (raw)
                sqlite3_close(raw);
            if (_err)
                *_err = mgpp::err{ code, msg };
            return nullptr;
        }

        details::sqlite_db_ptr db(raw);

        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db.get(), "SELECT lang, plural_rule FROM i18n_language",
                                -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            const int32_t code = mgec__from_sqlite3_err(rc);
            const memepp::string msg{ sqlite3_errmsg(db.get()) };
            if (stmt)
                sqlite3_finalize(stmt);
            if (_err)
                *_err = mgpp::err{ code, msg };
            return nullptr;
        }

        std::vector<lang_info> langs;
        for (;;) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE)
                break;
            if (rc != SQLITE_ROW) {
                const int32_t code = mgec__from_sqlite3_err(rc);
                const memepp::string msg{ sqlite3_errmsg(db.get()) };
                sqlite3_finalize(stmt);
                if (_err)
                    *_err = mgpp::err{ code, msg };
                return nullptr;
            }

            const unsigned char* lp = sqlite3_column_text(stmt, 0);
            const int ln = sqlite3_column_bytes(stmt, 0);
            const unsigned char* rp = sqlite3_column_text(stmt, 1);
            const int rn = sqlite3_column_bytes(stmt, 1);

            const char* lcp = lp ? reinterpret_cast<const char*>(lp) : "";
            const char* rcp = rp ? reinterpret_cast<const char*>(rp) : "";

            tag_t tag = tag_t::parse_from(
                memepp::string_view{ lcp, static_cast<memepp::string_view::size_type>(ln) });
            if (!tag.valid())
                continue;

            // Check for duplicate language tags
            for (const auto& existing : langs) {
                if (existing.tag == tag) {
                    sqlite3_finalize(stmt);
                    if (_err) {
                        std::string msg = "duplicate language tag: '";
                        msg.append(lcp, static_cast<size_t>(ln));
                        msg.append("' (same as '");
                        msg.append(existing.lang_text.data(), static_cast<size_t>(existing.lang_text.size()));
                        msg.append("')");
                        const memepp::string err_msg{ msg.c_str(),
                            static_cast<memepp::string::size_type>(msg.size()) };
                        *_err = mgpp::err{ MGEC__INVAL, err_msg };
                    }
                    return nullptr;
                }
            }

            lang_info li;
            li.tag = tag;
            li.lang_text = memepp::string{ lcp, static_cast<memepp::string::size_type>(ln) };
            li.rule = plural_rule::from_text_or_language(
                memepp::string_view{ rcp, static_cast<memepp::string_view::size_type>(rn) },
                tag);
            langs.push_back(std::move(li));
        }
        sqlite3_finalize(stmt);

        // Verify i18n_message table exists by preparing the actual query.
        // If this fails, every thread would fail on first prepare and keep
        // re-opening the file; catch it here instead.
        sqlite3_stmt* msg_stmt = nullptr;
        rc = sqlite3_prepare_v2(db.get(), details::conn_pool::select_sql(), -1, &msg_stmt, nullptr);
        if (rc != SQLITE_OK) {
            const int32_t code = mgec__from_sqlite3_err(rc);
            const memepp::string msg{ sqlite3_errmsg(db.get()) };
            if (msg_stmt)
                sqlite3_finalize(msg_stmt);
            if (_err)
                *_err = mgpp::err{ code, msg };
            return nullptr;
        }
        sqlite3_finalize(msg_stmt);

        auto cat = std::make_shared<lite_catalog>(lite_catalog::ctor_key{});
        cat->langs_ = std::move(langs);
        cat->pool_ = std::make_shared<details::conn_pool>(std::string{ _path_utf8 }, _opts);
        return cat;
    }

    size_t language_count() const noexcept { return langs_.size(); }

    bool has_language(const tag_t& _tag) const noexcept
    {
        return find_lang(_tag) != nullptr;
    }

    //! @brief Total number of connections currently open for this instance.
    size_t connection_count() const
    {
        return pool_ ? pool_->count() : 0;
    }

    //! @brief Close this thread's connection to this instance, if any.
    void release_thread_connection() const
    {
        if (!pool_)
            return;
        details::tls_conns& t = details::tls_conns::get();
        if (details::conn* c = t.unregister(pool_))
            pool_->close_conn(c);
    }

    //! @brief Number of LRU entries in this thread's cache (0 when no connection).
    size_t thread_cache_size() const
    {
        if (!pool_)
            return 0;
        details::conn* c = details::tls_conns::get().find(pool_);
        return c ? c->cache.size() : 0;
    }

protected:
    bool lookup_one(const tag_t& _lang, const memepp::string_view& _ctx,
                    const memepp::string_view& _msgid, bool _plural, uint64_t _n,
                    memepp::string& _out) const override
    {
        const lang_info* li = find_lang(_lang);
        if (!li)
            return false;   // language not declared: issue no SQL at all

        const uint32_t form = _plural ? li->rule.index(_n) : 0u;

        details::conn* c = details::acquire(pool_);
        if (!c)
            return false;

        std::string key;
        const bool use_cache = c->cache.enabled();
        if (use_cache) {
            key.reserve(static_cast<size_t>(li->lang_text.size())
                        + static_cast<size_t>(_ctx.size())
                        + static_cast<size_t>(_msgid.size()) + 24);
            key.append(li->lang_text.data(), static_cast<size_t>(li->lang_text.size()));
            key.push_back('\x1f');
            if (_ctx.size() > 0)
                key.append(_ctx.data(), static_cast<size_t>(_ctx.size()));
            key.push_back('\x04');
            if (_msgid.size() > 0)
                key.append(_msgid.data(), static_cast<size_t>(_msgid.size()));
            key.push_back('\x1f');
            key.append(std::to_string(form));

            if (const details::lru_cache::val_t* hit = c->cache.get(key)) {
                if (hit->found)
                    _out = hit->text;
                return hit->found;
            }
        }

        // An empty ctx must be bound as '' and never as NULL.
        const char* ctxp = _ctx.size() > 0 ? _ctx.data() : "";
        const char* idp = _msgid.size() > 0 ? _msgid.data() : "";

        sqlite3_bind_text(c->stmt, 1, li->lang_text.data(),
                          static_cast<int>(li->lang_text.size()), SQLITE_STATIC);
        sqlite3_bind_text(c->stmt, 2, ctxp, static_cast<int>(_ctx.size()), SQLITE_STATIC);
        sqlite3_bind_text(c->stmt, 3, idp, static_cast<int>(_msgid.size()), SQLITE_STATIC);
        sqlite3_bind_int64(c->stmt, 4, static_cast<sqlite3_int64>(form));

        bool found = false;
        memepp::string text;
        const int rc = sqlite3_step(c->stmt);
        if (rc == SQLITE_ROW) {
            const unsigned char* p = sqlite3_column_text(c->stmt, 0);
            const int nb = sqlite3_column_bytes(c->stmt, 0);   // must follow column_text()
            text = memepp::string{ p ? reinterpret_cast<const char*>(p) : "",
                                   static_cast<memepp::string::size_type>(nb) };
            found = true;
        }
        sqlite3_reset(c->stmt);
        sqlite3_clear_bindings(c->stmt);

        // Only cache a legitimate miss (SQLITE_DONE). A transient error such as
        // SQLITE_BUSY or SQLITE_IOERR must not be cached, otherwise one lock
        // conflict would pin the source text until the entry is evicted.
        if (use_cache && (rc == SQLITE_ROW || rc == SQLITE_DONE))
            c->cache.put(std::move(key), found, text);
        if (found)
            _out = std::move(text);
        return found;
    }

private:
    struct lang_info
    {
        tag_t          tag;
        memepp::string lang_text;   //!< the raw string from the db, bound verbatim
        plural_rule    rule;
    };

public:
    //! @brief Pass key: only lite_catalog itself can build one, so only open()
    //!        can construct a lite_catalog.
    struct ctor_key
    {
    private:
        ctor_key() = default;
        friend class lite_catalog;
    };

    explicit lite_catalog(ctor_key) {}

private:
    const lang_info* find_lang(const tag_t& _lang) const noexcept
    {
        for (const lang_info& li : langs_) {
            if (li.tag == _lang)
                return &li;
        }
        return nullptr;
    }

    std::vector<lang_info>              langs_;
    std::shared_ptr<details::conn_pool> pool_;
};

// ---------------------------------------------------------------------------
// fast_catalog_builder::load_from_sqlite
// ---------------------------------------------------------------------------
inline mgpp::err fast_catalog_builder::load_from_sqlite(const char* _path_utf8)
{
    if (!_path_utf8 || !*_path_utf8)
        return mgpp::err{ MGEC__INVAL, memepp::string{ "empty sqlite path" } };

    sqlite3* raw = nullptr;
    int rc = sqlite3_open_v2(_path_utf8, &raw, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        const int32_t code = mgec__from_sqlite3_err(rc);
        const memepp::string msg{ raw ? sqlite3_errmsg(raw) : "cannot open sqlite file" };
        if (raw)
            sqlite3_close(raw);
        return mgpp::err{ code, msg };
    }
    details::sqlite_db_ptr db(raw);

    // ---- 1. languages: raw db text -> tag ----
    std::unordered_map<std::string, tag_t> known;
    {
        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db.get(), "SELECT lang, plural_rule FROM i18n_language",
                                -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            const int32_t code = mgec__from_sqlite3_err(rc);
            const memepp::string msg{ sqlite3_errmsg(db.get()) };
            if (stmt)
                sqlite3_finalize(stmt);
            return mgpp::err{ code, msg };
        }

        // Map from tag to the first raw string that produced it, used to detect
        // duplicates (e.g. "zh-CN" and "zh_CN" both parse to the same tag).
        std::unordered_map<tag_t, std::string> tag_to_raw;

        for (;;) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE)
                break;
            if (rc != SQLITE_ROW) {
                const int32_t code = mgec__from_sqlite3_err(rc);
                const memepp::string msg{ sqlite3_errmsg(db.get()) };
                sqlite3_finalize(stmt);
                return mgpp::err{ code, msg };
            }

            const unsigned char* lp = sqlite3_column_text(stmt, 0);
            const int ln = sqlite3_column_bytes(stmt, 0);
            const unsigned char* rp = sqlite3_column_text(stmt, 1);
            const int rn = sqlite3_column_bytes(stmt, 1);

            const char* lcp = lp ? reinterpret_cast<const char*>(lp) : "";
            const char* rcp = rp ? reinterpret_cast<const char*>(rp) : "";

            tag_t tag = tag_t::parse_from(
                memepp::string_view{ lcp, static_cast<memepp::string_view::size_type>(ln) });
            if (!tag.valid())
                continue;

            // Check for duplicate language tags
            auto it = tag_to_raw.find(tag);
            if (it != tag_to_raw.end()) {
                sqlite3_finalize(stmt);
                std::string msg = "duplicate language tag: '";
                msg.append(lcp, static_cast<size_t>(ln));
                msg.append("' (same as '");
                msg.append(it->second);
                msg.append("')");
                const memepp::string err_msg{ msg.c_str(),
                    static_cast<memepp::string::size_type>(msg.size()) };
                return mgpp::err{ MGEC__INVAL, err_msg };
            }
            tag_to_raw.emplace(tag, std::string{ lcp, static_cast<size_t>(ln) });

            known[std::string{ lcp, static_cast<size_t>(ln) }] = tag;
            set_plural_rule(tag, memepp::string_view{
                rcp, static_cast<memepp::string_view::size_type>(rn) });
        }
        sqlite3_finalize(stmt);
    }

    // ---- 2. messages, grouped by (lang, ctx, msgid) ----
    {
        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db.get(),
            "SELECT lang, ctx, msgid, form, msgstr FROM i18n_message"
            " ORDER BY lang, ctx, msgid, form", -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            const int32_t code = mgec__from_sqlite3_err(rc);
            const memepp::string msg{ sqlite3_errmsg(db.get()) };
            if (stmt)
                sqlite3_finalize(stmt);
            return mgpp::err{ code, msg };
        }

        std::string cur_lang, cur_ctx, cur_id;
        bool        have_group = false;
        tag_t       cur_tag;
        std::vector<memepp::string> forms;
        std::vector<bool>           present;

        auto flush = [&]() {
            if (!have_group)
                return;
            put_forms(cur_tag,
                      memepp::string_view{ cur_ctx.data(),
                          static_cast<memepp::string_view::size_type>(cur_ctx.size()) },
                      memepp::string_view{ cur_id.data(),
                          static_cast<memepp::string_view::size_type>(cur_id.size()) },
                      std::move(forms), std::move(present));
            forms.clear();
            present.clear();
            have_group = false;
        };

        for (;;) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE)
                break;
            if (rc != SQLITE_ROW) {
                const int32_t code = mgec__from_sqlite3_err(rc);
                const memepp::string msg{ sqlite3_errmsg(db.get()) };
                sqlite3_finalize(stmt);
                return mgpp::err{ code, msg };
            }

            const unsigned char* lp = sqlite3_column_text(stmt, 0);
            const int ln = sqlite3_column_bytes(stmt, 0);
            const unsigned char* cp = sqlite3_column_text(stmt, 1);
            const int cn = sqlite3_column_bytes(stmt, 1);
            const unsigned char* ip = sqlite3_column_text(stmt, 2);
            const int in = sqlite3_column_bytes(stmt, 2);
            const sqlite3_int64 form = sqlite3_column_int64(stmt, 3);
            const unsigned char* mp = sqlite3_column_text(stmt, 4);
            const int mn = sqlite3_column_bytes(stmt, 4);

            const char* lcp = lp ? reinterpret_cast<const char*>(lp) : "";
            const char* ccp = cp ? reinterpret_cast<const char*>(cp) : "";
            const char* icp = ip ? reinterpret_cast<const char*>(ip) : "";
            const char* mcp = mp ? reinterpret_cast<const char*>(mp) : "";

            const std::string lang{ lcp, static_cast<size_t>(ln) };
            auto lit = known.find(lang);
            if (lit == known.end())
                continue;                 // not declared in i18n_language
            if (form < 0 || form >= 16)
                continue;                 // outside the supported range

            const std::string ctx{ ccp, static_cast<size_t>(cn) };
            const std::string id{ icp, static_cast<size_t>(in) };

            if (have_group && (lang != cur_lang || ctx != cur_ctx || id != cur_id))
                flush();

            if (!have_group) {
                cur_lang = lang;
                cur_ctx = ctx;
                cur_id = id;
                cur_tag = lit->second;
                have_group = true;
            }

            const size_t slot = static_cast<size_t>(form);
            if (forms.size() <= slot) {
                forms.resize(slot + 1);
                present.resize(slot + 1, false);
            }
            forms[slot] = memepp::string{ mcp, static_cast<memepp::string::size_type>(mn) };
            present[slot] = true;
        }

        flush();
        sqlite3_finalize(stmt);
    }

    return mgpp::err::make_ok();
}

#endif // MMBKPP_I18N_ENABLE_SQLITE

// ---------------------------------------------------------------------------
// catalog_registry / release_token
// ---------------------------------------------------------------------------
//! @brief Handle returned by catalog_registry::remove(), used to observe when the
//!        removed instance is really gone.
//!
//! @details The token holds a weak_ptr to the catalog. released() becomes true
//!          once every shared_ptr to that catalog (the registry's and yours) has
//!          been dropped. wait_released() polls until that happens or a timeout
//!          expires.
//!
//! Usage:
//! @code
//!   auto tk = registry.remove("plugin_x");
//!   if (!tk.released()) {
//!       my_own_shared_ptr.reset();                  // drop your reference too
//!       if (!tk.wait_released(std::chrono::milliseconds(1000))) {
//!           // something still holds the catalog: do NOT unload the DLL yet
//!       }
//!   }
//!   // tk.released() is true  ->  connections are closed, db file is free
//! @endcode
//!
//! @note A default-constructed token reports released() == true, which is what
//!       remove() returns for an unknown domain.
class release_token
{
public:
    release_token() noexcept = default;

    bool released() const noexcept { return wp_.expired(); }

    bool wait_released(std::chrono::milliseconds _timeout) const
    {
        if (released())
            return true;

        const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + _timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (released())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return released();
    }

private:
    friend class catalog_registry;

    std::weak_ptr<const catalog> wp_;
};

//! @brief Holds several catalogs and dispatches each query to them in insertion
//!        order, so several domains (host + one per plugin) can coexist.
//!
//! @details Reads are lock-free: every mutation publishes a fresh immutable
//!          snapshot through an atomic shared_ptr, and a query only loads that
//!          snapshot. The first catalog that has the entry wins; when none has it,
//!          the original text is returned.
//!
//! Usage:
//! @code
//!   catalog_registry reg;
//!   reg.add("host", host_catalog);        // added first -> highest priority
//!   reg.add("plugin_x", plugin_catalog);
//!
//!   // Query across all catalogs (falls back to the original text)
//!   reg.tr(make_tag("zh-CN"), "Open");
//!   reg.tr_n(make_tag("ru"), "%d file", "%d files", 5);
//!
//!   // Tag-less overloads use the registry's own default language
//!   reg.set_default_language(make_tag("zh-CN"));
//!   reg.tr("Open");
//!
//!   // Query one domain only, to avoid cross-domain msgid clashes
//!   if (auto c = reg.get("plugin_x")) {
//!       c->tr(make_tag("zh-CN"), "Open");
//!   }
//!
//!   // --- Global instance used by the free functions ---
//!   auto reg2 = std::make_shared<catalog_registry>();
//!   reg2->add("host", host_catalog);
//!   set_global(reg2);
//!   mmbkpp::i18n::tr("Open");             // forwards to global()
//!   set_global(nullptr);                  // restore the built-in instance
//!
//!   // --- Unload a plugin ---
//!   auto tk = reg.remove("plugin_x");
//!   tk.wait_released(std::chrono::milliseconds(1000));
//! @endcode
//!
//! @note The free functions tr() / tr_ctx() / tr_n() / tr_ctx_n() all forward to
//!       global(), which returns a shared_ptr to the built-in registry until you
//!       call set_global(). Each free-function call takes a strong reference for
//!       its duration, so a concurrent set_global() cannot destroy the registry
//!       mid-query.
class catalog_registry
{
public:
    catalog_registry()
        : default_(make_tag("en"))
    {
        snap_.store(std::make_shared<const snapshot_t>());
    }

    catalog_registry(const catalog_registry&) = delete;
    catalog_registry& operator=(const catalog_registry&) = delete;

    bool add(const memepp::string_view& _domain, std::shared_ptr<catalog> _cat)
    {
        if (!_cat)
            return false;

        std::lock_guard<std::mutex> lk(wmtx_);
        std::shared_ptr<const snapshot_t> old = snap_.load();
        if (old) {
            for (const entry_t& e : *old) {
                if (memepp::string_view{ e.first } == _domain)
                    return false;
            }
        }

        std::shared_ptr<snapshot_t> nw =
            std::make_shared<snapshot_t>(old ? *old : snapshot_t{});
        nw->emplace_back(_domain.to_string(), std::move(_cat));
        snap_.store(std::shared_ptr<const snapshot_t>(std::move(nw)));
        return true;
    }

    release_token remove(const memepp::string_view& _domain)
    {
        release_token tk;
        std::shared_ptr<catalog> removed;

        {
            std::lock_guard<std::mutex> lk(wmtx_);
            std::shared_ptr<const snapshot_t> old = snap_.load();
            if (!old)
                return tk;

            std::shared_ptr<snapshot_t> nw = std::make_shared<snapshot_t>();
            nw->reserve(old->size());

            bool found = false;
            for (const entry_t& e : *old) {
                if (memepp::string_view{ e.first } == _domain) {
                    removed = e.second;
                    found = true;
                    continue;
                }
                nw->push_back(e);
            }
            if (!found)
                return tk;

            snap_.store(std::shared_ptr<const snapshot_t>(std::move(nw)));
        }

        // Hand the removed instance to the caller outside the lock: its destructor
        // may be slow and must not block readers.
        tk.wp_ = removed;
        return tk;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lk(wmtx_);
        snap_.store(std::make_shared<const snapshot_t>());
    }

    std::shared_ptr<catalog> get(const memepp::string_view& _domain) const
    {
        std::shared_ptr<const snapshot_t> s = snap_.load();
        if (s) {
            for (const entry_t& e : *s) {
                if (memepp::string_view{ e.first } == _domain)
                    return e.second;
            }
        }
        return nullptr;
    }

    size_t size() const
    {
        std::shared_ptr<const snapshot_t> s = snap_.load();
        return s ? s->size() : 0;
    }

    void set_default_language(const tag_t& _tag)
    {
        default_.store(_tag);
    }

    tag_t default_language() const
    {
        return default_.load();
    }

    // ---- queries: same names and signatures as catalog ----
    memepp::string tr(const memepp::string_view& _msgid) const
    {
        return tr(default_language(), _msgid);
    }

    memepp::string tr(const tag_t& _tag, const memepp::string_view& _msgid) const
    {
        memepp::string out;
        if (dispatch(_tag, memepp::string_view{}, _msgid, false, 0, out))
            return out;
        return _msgid.to_string();
    }

    memepp::string tr_ctx(const memepp::string_view& _ctx,
                          const memepp::string_view& _msgid) const
    {
        return tr_ctx(default_language(), _ctx, _msgid);
    }

    memepp::string tr_ctx(const tag_t& _tag, const memepp::string_view& _ctx,
                          const memepp::string_view& _msgid) const
    {
        memepp::string out;
        if (dispatch(_tag, _ctx, _msgid, false, 0, out))
            return out;
        return _msgid.to_string();
    }

    memepp::string tr_n(const memepp::string_view& _msgid,
                        const memepp::string_view& _msgid_plural, uint64_t _n) const
    {
        return tr_n(default_language(), _msgid, _msgid_plural, _n);
    }

    memepp::string tr_n(const tag_t& _tag, const memepp::string_view& _msgid,
                        const memepp::string_view& _msgid_plural, uint64_t _n) const
    {
        return tr_ctx_n(_tag, memepp::string_view{}, _msgid, _msgid_plural, _n);
    }

    memepp::string tr_ctx_n(const memepp::string_view& _ctx, const memepp::string_view& _msgid,
                            const memepp::string_view& _msgid_plural, uint64_t _n) const
    {
        return tr_ctx_n(default_language(), _ctx, _msgid, _msgid_plural, _n);
    }

    memepp::string tr_ctx_n(const tag_t& _tag, const memepp::string_view& _ctx,
                            const memepp::string_view& _msgid,
                            const memepp::string_view& _msgid_plural, uint64_t _n) const
    {
        memepp::string out;
        if (dispatch(_tag, _ctx, _msgid, true, _n, out))
            return out;
        return _n == 1 ? _msgid.to_string() : _msgid_plural.to_string();
    }

    // ---- misuse guard ----
    memepp::string tr(const memepp::string_view&, const memepp::string_view&) const = delete;
    memepp::string tr_ctx(const memepp::string_view&, const memepp::string_view&,
                          const memepp::string_view&) const = delete;
    memepp::string tr_n(const memepp::string_view&, const memepp::string_view&,
                        const memepp::string_view&, uint64_t) const = delete;
    memepp::string tr_ctx_n(const memepp::string_view&, const memepp::string_view&,
                            const memepp::string_view&, const memepp::string_view&,
                            uint64_t) const = delete;

private:
    using entry_t    = std::pair<memepp::string, std::shared_ptr<catalog>>;
    using snapshot_t = std::vector<entry_t>;

    //! @brief First hit wins, in add() order.
    bool dispatch(const tag_t& _tag, const memepp::string_view& _ctx,
                  const memepp::string_view& _msgid, bool _plural, uint64_t _n,
                  memepp::string& _out) const
    {
        std::shared_ptr<const snapshot_t> s = snap_.load();
        if (!s)
            return false;
        for (const entry_t& e : *s) {
            if (e.second && e.second->lookup(_tag, _ctx, _msgid, _plural, _n, _out))
                return true;
        }
        return false;
    }

    mutable std::mutex                             wmtx_;
    mgpp::mem::atomic_shared_ptr<const snapshot_t> snap_;
    mgpp::thrd::seqlock<tag_t>                     default_;
};

// ---------------------------------------------------------------------------
// Global singleton and free functions
// ---------------------------------------------------------------------------
namespace details {

//! @brief The built-in registry used whenever no registry has been installed.
//!        Held in a shared_ptr so that global() can return a strong reference.
inline std::shared_ptr<catalog_registry>& builtin_registry()
{
    static std::shared_ptr<catalog_registry> r = std::make_shared<catalog_registry>();
    return r;
}

//! @brief Currently installed registry (empty means "use the built-in one").
inline mgpp::mem::atomic_shared_ptr<catalog_registry>& global_ptr() noexcept
{
    static mgpp::mem::atomic_shared_ptr<catalog_registry> p;
    return p;
}

} // namespace details

//! @brief The registry the free functions forward to.
//! @return A strong reference: the registry cannot be destroyed while the caller
//!         still holds the returned pointer, so a concurrent set_global() cannot
//!         pull the object out from under an in-flight query.
inline std::shared_ptr<catalog_registry> global() noexcept
{
    std::shared_ptr<catalog_registry> p = details::global_ptr().load();
    return p ? p : details::builtin_registry();
}

//! @brief Install _reg as the global registry. A null pointer restores the
//!        built-in instance. Takes ownership of _reg.
inline void set_global(std::shared_ptr<catalog_registry> _reg) noexcept
{
    details::global_ptr().store(std::move(_reg));
}

inline memepp::string tr(const memepp::string_view& _msgid)
{
    return global()->tr(_msgid);
}

inline memepp::string tr(const tag_t& _tag, const memepp::string_view& _msgid)
{
    return global()->tr(_tag, _msgid);
}

inline memepp::string tr_ctx(const memepp::string_view& _ctx, const memepp::string_view& _msgid)
{
    return global()->tr_ctx(_ctx, _msgid);
}

inline memepp::string tr_ctx(const tag_t& _tag, const memepp::string_view& _ctx,
                             const memepp::string_view& _msgid)
{
    return global()->tr_ctx(_tag, _ctx, _msgid);
}

inline memepp::string tr_n(const memepp::string_view& _msgid,
                           const memepp::string_view& _msgid_plural, uint64_t _n)
{
    return global()->tr_n(_msgid, _msgid_plural, _n);
}

inline memepp::string tr_n(const tag_t& _tag, const memepp::string_view& _msgid,
                           const memepp::string_view& _msgid_plural, uint64_t _n)
{
    return global()->tr_n(_tag, _msgid, _msgid_plural, _n);
}

inline memepp::string tr_ctx_n(const memepp::string_view& _ctx, const memepp::string_view& _msgid,
                               const memepp::string_view& _msgid_plural, uint64_t _n)
{
    return global()->tr_ctx_n(_ctx, _msgid, _msgid_plural, _n);
}

inline memepp::string tr_ctx_n(const tag_t& _tag, const memepp::string_view& _ctx,
                               const memepp::string_view& _msgid,
                               const memepp::string_view& _msgid_plural, uint64_t _n)
{
    return global()->tr_ctx_n(_tag, _ctx, _msgid, _msgid_plural, _n);
}

inline memepp::string tr(const memepp::string_view&, const memepp::string_view&) = delete;
inline memepp::string tr_ctx(const memepp::string_view&, const memepp::string_view&,
                             const memepp::string_view&) = delete;
inline memepp::string tr_n(const memepp::string_view&, const memepp::string_view&,
                           const memepp::string_view&, uint64_t) = delete;
inline memepp::string tr_ctx_n(const memepp::string_view&, const memepp::string_view&,
                               const memepp::string_view&, const memepp::string_view&,
                               uint64_t) = delete;

} // namespace i18n
} // namespace mmbkpp

#endif // !MMBKPP_I18N_CATALOG_H_INCLUDED
