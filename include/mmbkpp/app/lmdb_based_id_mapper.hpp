
#ifndef MMUPP_APP_LMDB_BASED_ID_MAPPER_HPP_INCLUDED
#define MMUPP_APP_LMDB_BASED_ID_MAPPER_HPP_INCLUDED

#include <memepp/string.hpp>
#include <memepp/buffer_view.hpp>
#include <memepp/convert/std/string.hpp>
#include <megopp/util/scope_cleanup.h>
#include <megopp/err/err.hpp>
#include <mmutilspp/fs/program_path.hpp>
#include "lmdb_env.hpp"

#include <mego/err/ec_impl.h>
#include <liblmdb/lmdb.h>
#include <ghc/filesystem.hpp>

#include <tuple>
#include <mutex>
#include <memory>
#include <functional>
#include <atomic>

namespace mmbkpp { namespace app {

template <typename _NumTy>
class lmdb_based_id_mapper
{
public:
    using num_t = _NumTy;
    using num_generator_t = std::function<num_t(
        const memepp::buffer_view& _str_id, size_t _item_size)>;

    lmdb_based_id_mapper();
    ~lmdb_based_id_mapper();

    void set_dir_path(const memepp::string_view& _dir_path);
    void set_db_mapsize(size_t _db_mapsize);
    void set_num_generator(const num_generator_t& _num_generator);

    inline memepp::string dir_path() const { 
        std::lock_guard<std::mutex> lock(mtx_);
        return dir_path_; 
    }
    inline size_t db_mapsize() const { 
        std::lock_guard<std::mutex> lock(mtx_);
        return db_mapsize_; 
    }

    std::tuple<memepp::string, mgpp::err> get_id(const num_t& _id) const;
    std::tuple<num_t, mgpp::err> get_id(const memepp::buffer_view& _id) const;

    mgpp::err insert_id(const memepp::buffer_view& _str_id, const num_t& _num_id);
    std::tuple<num_t, mgpp::err> insert_id(const memepp::buffer_view& _id);

    mgpp::err remove_id(const memepp::buffer_view& _str_id, const num_t& _num_id);
    mgpp::err remove_id(const memepp::buffer_view& _str_id);
    mgpp::err remove_id(const num_t& _num_id);

private:

    std::tuple<lmdb_env_ptr, mgpp::err> __get_strnum_env() const;
    std::tuple<lmdb_env_ptr, mgpp::err> __get_numstr_env() const;
    mgpp::err __open_envs() const;

    mutable std::mutex mtx_;
    memepp::string dir_path_;
    memepp::string subdir_;
    size_t db_mapsize_;
    std::shared_ptr<num_generator_t> num_generator_;
    mutable lmdb_env_ptr strnum_env_;
    mutable lmdb_env_ptr numstr_env_;
    std::atomic<num_t> self_incr_index_;
};

template <typename _NumTy>
inline lmdb_based_id_mapper<_NumTy>::lmdb_based_id_mapper():
    dir_path_(mmbkpp::paths::relative_with_program_path("id_mapps")),
    subdir_("id_mapp"),
    db_mapsize_(10485760),
    self_incr_index_(std::numeric_limits<typename std::make_unsigned<num_t>::type>::max())
{
}

template <typename _NumTy>
inline lmdb_based_id_mapper<_NumTy>::~lmdb_based_id_mapper()
{
}

template <typename _NumTy>
inline void lmdb_based_id_mapper<_NumTy>::set_dir_path(const memepp::string_view& _dir_path)
{
    std::lock_guard<std::mutex> lock(mtx_);
    dir_path_ = _dir_path.to_string();
}

template <typename _NumTy>
inline void lmdb_based_id_mapper<_NumTy>::set_db_mapsize(size_t _db_mapsize)
{
    std::lock_guard<std::mutex> lock(mtx_);
    db_mapsize_ = _db_mapsize;
}

template <typename _NumTy>
inline void lmdb_based_id_mapper<_NumTy>::set_num_generator(const num_generator_t& _num_generator)
{
    std::lock_guard<std::mutex> lock(mtx_);
    num_generator_ = std::make_shared<num_generator_t>(_num_generator);
}

template <typename _NumTy>
inline std::tuple<memepp::string, mgpp::err> lmdb_based_id_mapper<_NumTy>::get_id(const num_t& _id) const
{
    auto [env, err] = __get_numstr_env();
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return std::make_tuple(memepp::string{}, err);
    }

    uint8_t key_buf[sizeof(_id)] = { 0 };
    for (size_t i = 0; i < sizeof(_id); ++i) {
        key_buf[i] = (_id >> (i * 8)) & 0xff;
    }
    ::MDB_val key = { sizeof(_id), key_buf };
    ::MDB_val val = { 0, nullptr };

    err = env->do_read(
        [&env, &key, &val](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
        {
            int ec = ::mdb_get(_txn, _dbi, &key, &val);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to get lmdb value for lmdb_based_id_mapper" };
            }
            return mgpp::err::make_ok();
        });
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return std::make_tuple(memepp::string{}, err);
    }

    return std::make_tuple(memepp::string{ (const char*)val.mv_data, mmint_t(val.mv_size) }, mgpp::err::make_ok());
}

template <typename _NumTy>
inline std::tuple<typename lmdb_based_id_mapper<_NumTy>::num_t, mgpp::err> 
    lmdb_based_id_mapper<_NumTy>::get_id(const memepp::buffer_view& _id) const
{
    auto [env, err] = __get_strnum_env();
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return std::make_tuple(num_t{ -1 }, err);
    }

    ::MDB_val key = { size_t(_id.size()), (void*)_id.data()};
    ::MDB_val val = { 0, nullptr };

    err = env->do_read(
        [&env, &key, &val](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
        {
            int ec = ::mdb_get(_txn, _dbi, &key, &val);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to get lmdb value for lmdb_based_id_mapper" };
            }
            return mgpp::err::make_ok();
        });

    num_t num_id = 0;
    for (size_t i = 0; i < sizeof(num_id); ++i) {
        num_id |= (num_t)((uint8_t*)val.mv_data)[i] << (i * 8);
    }

    return std::make_tuple(num_id, mgpp::err::make_ok());
}

template <typename _NumTy>
inline mgpp::err lmdb_based_id_mapper<_NumTy>::insert_id(const memepp::buffer_view& _str_id, const num_t& _num_id)
{
    auto [strnum_env, strnum_err] = __get_strnum_env();
    if (MEGO_SYMBOL__UNLIKELY(strnum_err)) {
        return strnum_err;
    }

    auto [numstr_env, numstr_err] = __get_numstr_env();
    if (MEGO_SYMBOL__UNLIKELY(numstr_err)) {
        return numstr_err;
    }

    int ec = 0;
    mgpp::err err = mgpp::err::make_ok();

    uint8_t key_buf[sizeof(_num_id)] = { 0 };
    for (size_t i = 0; i < sizeof(_num_id); ++i) {
        key_buf[i] = (_num_id >> (i * 8)) & 0xff;
    }

    ::MDB_val strnum_key = { _str_id.size(), (void*)_str_id.data() };
    ::MDB_val strnum_val = { sizeof(_num_id), key_buf };

    ::MDB_txn* strnum_txn = nullptr;
    ::MDB_dbi  strnum_dbi = 0;
    err = strnum_env->do_write_begin(strnum_txn, strnum_dbi,
        [&strnum_env, &strnum_key, &strnum_val](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
        {
            int ec = ::mdb_put(_txn, _dbi, &strnum_key, &strnum_val, 0);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to put lmdb value for lmdb_based_id_mapper" };
            }
            return mgpp::err::make_ok();
        });
    auto strnum_dbi_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (strnum_dbi != 0) {
            ::mdb_dbi_close(strnum_env->native_handle(), strnum_dbi);
        }
    });
    auto strnum_txn_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (strnum_txn != nullptr) {
            ::mdb_txn_abort(strnum_txn);
        }
    });
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return err;
    }

    ::MDB_val numstr_key = { sizeof(_num_id), key_buf };
    ::MDB_val numstr_val = { _str_id.size(), (void*)_str_id.data() };

    ::MDB_txn* numstr_txn = nullptr;
    ::MDB_dbi  numstr_dbi = 0;
    err = numstr_env->do_write_begin(numstr_txn, numstr_dbi,
        [&numstr_env, &numstr_key, &numstr_val](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
        {
            int ec = ::mdb_put(_txn, _dbi, &numstr_key, &numstr_val, 0);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to put lmdb value for lmdb_based_id_mapper" };
            }
            return mgpp::err::make_ok();
        });
    auto numstr_dbi_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (numstr_dbi != 0) {
            ::mdb_dbi_close(numstr_env->native_handle(), numstr_dbi);
        }
    });
    auto numstr_txn_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (numstr_txn != nullptr) {
            ::mdb_txn_abort(numstr_txn);
        }
    });
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return err;
    }

    ec = ::mdb_txn_commit(strnum_txn);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return mgpp::err{ MGEC__ERR, 
            "failed to commit lmdb txn for lmdb_based_id_mapper" };
    }

    strnum_txn_cleanup.cancel();
    strnum_dbi_cleanup.early_exec();

    ec = ::mdb_txn_commit(numstr_txn);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        // remove strnum record
        strnum_env->do_write(
            [&strnum_env, &strnum_key](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
            {
                int ec = ::mdb_del(_txn, _dbi, &strnum_key, nullptr);
                if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                    return mgpp::err{ MGEC__ERR, 
                        "failed to del lmdb value for lmdb_based_id_mapper" };
                }
                return mgpp::err::make_ok();
            });

        return mgpp::err{ MGEC__ERR, 
            "failed to commit lmdb txn for lmdb_based_id_mapper" };
    }

    numstr_txn_cleanup.cancel();
    return mgpp::err::make_ok();
}

template <typename _NumTy>
inline std::tuple<typename lmdb_based_id_mapper<_NumTy>::num_t, mgpp::err>
    lmdb_based_id_mapper<_NumTy>::insert_id(const memepp::buffer_view& _str_id)
{   
    int ec = 0;
    mgpp::err err = mgpp::err::make_ok();
    auto [strnum_env, strnum_err] = __get_strnum_env();
    err = strnum_err;
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return std::make_tuple(num_t{ -1 }, err);
    }

    auto [numstr_env, numstr_err] = __get_numstr_env();
    err = numstr_err;
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return std::make_tuple(num_t{ -1 }, err);
    }

    num_t num_id = -1;
    uint8_t strnum_buf[sizeof(num_id)] = { 0 };
    ::MDB_val strnum_key = { 0 };
    ::MDB_val strnum_val = { 0 };

    ::MDB_txn* strnum_txn = nullptr;
    ::MDB_dbi  strnum_dbi = 0;
    err = strnum_env->do_write_begin(strnum_txn, strnum_dbi,
        [&](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
        {
            ::MDB_stat stat;
            ec = ::mdb_stat(_txn, _dbi, &stat);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to stat lmdb for lmdb_based_id_mapper" };
            }

            if (num_generator_) {
                num_id = (*num_generator_)(_str_id, stat.ms_entries);
            }
            else {
                const char* name = "__INTERVAL__.SELF_INCR_INDEX";
                num_t data = SIZE_MAX;
                ::MDB_val key = { strlen(name), (void*)name };
                ::MDB_val val = { 0, nullptr };

                if(self_incr_index_ == std::numeric_limits<typename std::make_unsigned<num_t>::type>::max())
                {

                    ec = ::mdb_get(_txn, _dbi, &key, &val);
                    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                        return mgpp::err{ MGEC__ERR, 
                            "failed to get lmdb value for lmdb_based_id_mapper" };
                    }

                    if (val.mv_size == sizeof(data)) {
                        for (size_t i = 0; i < sizeof(data); ++i)
                            data |= (size_t)((uint8_t*)val.mv_data)[i] << (i * 8);
                    }
                    self_incr_index_ = data;
                }

                (++self_incr_index_ / std::numeric_limits<num_t>::max() == 0) ? num_id = self_incr_index_ : num_id = 0;

                uint8_t data_buf[sizeof(data)] = { 0 };
                for (size_t i = 0; i < sizeof(data); ++i)
                    data_buf[i] = (data >> (i * 8)) & 0xff;
                key = { strlen(name), (void*)name };
                val = { sizeof(data), data_buf };

                ec = ::mdb_put(_txn, _dbi, &key, &val, 0);
                if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                    return mgpp::err{ MGEC__ERR, 
                        "failed to put lmdb value for lmdb_based_id_mapper" };
                }
            }
            if (num_id == num_t(-1)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to generate num_id for lmdb_based_id_mapper" };
            }

            for (size_t i = 0; i < sizeof(num_id); ++i)
                strnum_buf[i] = (num_id >> (i * 8)) & 0xff;

            ::MDB_val strnum_key = { size_t(_str_id.size()), (void*)_str_id.data()};
            ::MDB_val strnum_val = { sizeof(num_id), strnum_buf };

            ec = ::mdb_put(_txn, _dbi, &strnum_key, &strnum_val, 0);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to put lmdb value for lmdb_based_id_mapper" };
            }
            return mgpp::err::make_ok();
        });
    auto strnum_dbi_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (strnum_dbi != 0) {
            ::mdb_dbi_close(strnum_env->native_handle(), strnum_dbi);
        }
    });
    auto strnum_txn_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (strnum_txn != nullptr) {
            ::mdb_txn_abort(strnum_txn);
        }
    });
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return std::make_tuple(num_t{ -1 }, err);
    }

    ::MDB_val numstr_key = { sizeof(num_id), strnum_buf };
    ::MDB_val numstr_val = { size_t(_str_id.size()), (void*)_str_id.data()};

    ::MDB_txn* numstr_txn = nullptr;
    ::MDB_dbi  numstr_dbi = 0;
    err = numstr_env->do_write_begin(numstr_txn, numstr_dbi,
        [this, &numstr_key, &numstr_val](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
        {
            int ec = ::mdb_put(_txn, _dbi, &numstr_key, &numstr_val, 0);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to put lmdb value for lmdb_based_id_mapper" };
            }
            return mgpp::err::make_ok();
        });
    auto numstr_dbi_cleanup = megopp::util::scope_cleanup__create(
        [&]() {
        if (numstr_dbi != 0)
            ::mdb_dbi_close(numstr_env->native_handle(), numstr_dbi);
        });
    auto numstr_txn_cleanup = megopp::util::scope_cleanup__create(
        [&]() {
        if (numstr_txn != nullptr)
            ::mdb_txn_abort(numstr_txn);
        });
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return std::make_tuple(num_t{ -1 }, err);
    }

    ec = ::mdb_txn_commit(strnum_txn);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return std::make_tuple(num_t{ -1 }, mgpp::err{ MGEC__ERR, 
            "failed to commit lmdb txn for lmdb_based_id_mapper" });
    }
    
    strnum_dbi_cleanup.early_exec();
    strnum_txn_cleanup.cancel();

    ec = ::mdb_txn_commit(numstr_txn);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        // remove strnum record
        strnum_env->do_write(
            [&strnum_key](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
            {
                int ec = ::mdb_del(_txn, _dbi, &strnum_key, nullptr);
                if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                    return mgpp::err{ MGEC__ERR, 
                        "failed to del lmdb value for lmdb_based_id_mapper" };
                }
                return mgpp::err::make_ok();
            });

        return std::make_tuple(num_t{ -1 }, mgpp::err{ MGEC__ERR, 
            "failed to commit lmdb txn for lmdb_based_id_mapper" });
    }

    numstr_txn_cleanup.cancel();
    return std::make_tuple(num_id, mgpp::err::make_ok());
}

template <typename _NumTy>
inline mgpp::err lmdb_based_id_mapper<_NumTy>::remove_id(const memepp::buffer_view& _str_id, const num_t& _num_id)
{
    int ec = 0;
    mgpp::err err = mgpp::err::make_ok();
    auto [strnum_env, strnum_err] = __get_strnum_env();
    err = strnum_err;
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return err;
    }

    auto [numstr_env, numstr_err] = __get_numstr_env();
    err = numstr_err;
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return err;
    }

    uint8_t key_buf[sizeof(_num_id)] = { 0 };
    for (size_t i = 0; i < sizeof(_num_id); ++i) {
        key_buf[i] = (_num_id >> (i * 8)) & 0xff;
    }

    ::MDB_val strnum_key = { size_t(_str_id.size()), (void*)_str_id.data() };
    ::MDB_val strnum_val = { sizeof(_num_id), key_buf };

    ::MDB_txn* strnum_txn = nullptr;
    ::MDB_dbi  strnum_dbi = 0;
    err = strnum_env->do_write_begin(strnum_txn, strnum_dbi,
        [&](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
        {
            ec = ::mdb_del(_txn, _dbi, &strnum_key, nullptr);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to del lmdb value for lmdb_based_id_mapper" };
            }

            return mgpp::err::make_ok();
        });
    auto strnum_dbi_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (strnum_dbi != 0)
            ::mdb_dbi_close(strnum_env->native_handle(), strnum_dbi);
        });
    auto strnum_txn_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (strnum_txn != nullptr)
            ::mdb_txn_abort(strnum_txn);
        });
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return err;
    }

    ::MDB_val numstr_key = { sizeof(_num_id), key_buf };
    // ::MDB_val numstr_val = { 0, nullptr };

    ::MDB_txn* numstr_txn = nullptr;
    ::MDB_dbi  numstr_dbi = 0;
    err = numstr_env->do_write_begin(numstr_txn, numstr_dbi,
        [&](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
        {
            ec = ::mdb_del(_txn, _dbi, &numstr_key, nullptr);
            if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                return mgpp::err{ MGEC__ERR, 
                    "failed to del lmdb value for lmdb_based_id_mapper" };
            }

            return mgpp::err::make_ok();
        });
    auto numstr_dbi_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (numstr_dbi != 0)
            ::mdb_dbi_close(numstr_env->native_handle(), numstr_dbi);
        });
    auto numstr_txn_cleanup = megopp::util::scope_cleanup__create([&]() {
        if (numstr_txn != nullptr)
            ::mdb_txn_abort(numstr_txn);
        });
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return err;
    }

    ec = ::mdb_txn_commit(strnum_txn);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return mgpp::err{ MGEC__ERR, 
            "failed to commit lmdb txn for lmdb_based_id_mapper" };
    }

    strnum_txn_cleanup.cancel();
    strnum_dbi_cleanup.early_exec();

    ec = ::mdb_txn_commit(numstr_txn);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        // rollback strnum record
        strnum_env->do_write(
            [&](::MDB_env* _env, ::MDB_txn* _txn, ::MDB_dbi _dbi) -> mgpp::err 
            {
                int ec = ::mdb_put(_txn, _dbi, &strnum_key, &strnum_val, 0);
                if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
                    return mgpp::err{ MGEC__ERR, 
                        "failed to put lmdb value for lmdb_based_id_mapper" };
                }
                return mgpp::err::make_ok();
            });

        return mgpp::err{ MGEC__ERR, 
            "failed to commit lmdb txn for lmdb_based_id_mapper" };
    }

    numstr_txn_cleanup.cancel();
    return mgpp::err::make_ok();
}

template <typename _NumTy>
inline mgpp::err lmdb_based_id_mapper<_NumTy>::remove_id(const memepp::buffer_view& _str_id)
{
    auto [num, err] = get_id(_str_id);
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return err;
    }
    return remove_id(_str_id, num);
}

template <typename _NumTy>
inline mgpp::err lmdb_based_id_mapper<_NumTy>::remove_id(const num_t& _num_id)
{
    auto [str, err] = get_id(_num_id);
    if (MEGO_SYMBOL__UNLIKELY(err)) {
        return err;
    }
    return remove_id(str, _num_id);
}

template <typename _NumTy>
inline std::tuple<lmdb_env_ptr, mgpp::err> lmdb_based_id_mapper<_NumTy>::__get_strnum_env() const
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (!strnum_env_) {
        locker.unlock();
        auto err = __open_envs();
        if (MEGO_SYMBOL__UNLIKELY(err)) {
            return std::make_tuple(nullptr, err);
        }
        locker.lock();
    }
    return std::make_tuple(strnum_env_, mgpp::err::make_ok());
}

template <typename _NumTy>
inline std::tuple<lmdb_env_ptr, mgpp::err> lmdb_based_id_mapper<_NumTy>::__get_numstr_env() const
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (!numstr_env_) {
        locker.unlock();
        auto err = __open_envs();
        if (MEGO_SYMBOL__UNLIKELY(err)) {
            return std::make_tuple(nullptr, err);
        }
        locker.lock();
    }
    return std::make_tuple(numstr_env_, mgpp::err::make_ok());
}

template <typename _NumTy>
inline mgpp::err lmdb_based_id_mapper<_NumTy>::__open_envs() const
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (strnum_env_ && numstr_env_)
        return mgpp::err::make_ok();
    
    auto strnum_db_path = memepp::c_format(2048, "%s/%s/strnum.%s",
        dir_path_.c_str(), subdir_.c_str(), "db");

    auto numstr_db_path = memepp::c_format(2048, "%s/%s/numstr.%s",
        dir_path_.c_str(), subdir_.c_str(), "db");

    std::error_code errc;
    ghc::filesystem::create_directories(mm_to<std::string>(dir_path_), errc);
    if (errc) {
        return mgpp::err{ mgec__from_posix_err(int(errc.value())), 
            "failed to create directory for lmdb_based_id_mapper" };
    }

    ::MDB_env* strnum_env = nullptr;
    auto ec = ::mdb_env_create(&strnum_env);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return mgpp::err{ MGEC__ERR, 
            "failed to create lmdb env for lmdb_based_id_mapper" };
    }

    auto strnum_env_cleanup = megopp::util::scope_cleanup__create(
        [&] { 
            ::mdb_env_close(strnum_env); 
            ghc::filesystem::remove(mm_to<std::string>(strnum_db_path));
        });

    ec = ::mdb_env_set_mapsize(strnum_env, db_mapsize_);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return mgpp::err{ MGEC__ERR, 
            "failed to set mapsize for lmdb env for lmdb_based_id_mapper" };
    }

#if defined(MDBX_LIFORECLAIM)
        ec = ::mdb_env_open(strnum_env, strnum_db_path.c_str(), MDB_CREATE | MDB_WRITEMAP | MDBX_LIFORECLAIM, 0664);
#else
        ec = ::mdb_env_open(strnum_env, strnum_db_path.c_str(), MDB_CREATE | MDB_WRITEMAP, 0664);
#endif

    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return mgpp::err{ MGEC__ERR, 
            "failed to open lmdb env for lmdb_based_id_mapper" };
    }

    ::MDB_env* numstr_env = nullptr;
    ec = ::mdb_env_create(&numstr_env);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return mgpp::err{ MGEC__ERR, 
            "failed to create lmdb env for lmdb_based_id_mapper" };
    }

    auto numstr_env_cleanup = megopp::util::scope_cleanup__create(
        [&]() { 
            ::mdb_env_close(numstr_env); 
            ghc::filesystem::remove(mm_to<std::string>(numstr_db_path));
        });
    
    ec = ::mdb_env_set_mapsize(numstr_env, db_mapsize_);
    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return mgpp::err{ MGEC__ERR, 
            "failed to set mapsize for lmdb env for lmdb_based_id_mapper" };
    }

#if defined(MDBX_LIFORECLAIM)
        ec = ::mdb_env_open(numstr_env, numstr_db_path.c_str(), MDB_CREATE | MDB_WRITEMAP | MDBX_LIFORECLAIM, 0664);
#else
        ec = ::mdb_env_open(numstr_env, numstr_db_path.c_str(), MDB_CREATE | MDB_WRITEMAP, 0664);
#endif

    if (MEGO_SYMBOL__UNLIKELY(ec != 0)) {
        return mgpp::err{ MGEC__ERR, 
            "failed to open lmdb env for lmdb_based_id_mapper" };
    }

    auto strnum_env_ptr = std::make_shared<lmdb_env>(strnum_env);
    auto numstr_env_ptr = std::make_shared<lmdb_env>(numstr_env);

    strnum_env_cleanup.cancel();
    numstr_env_cleanup.cancel();

    strnum_env_ = std::move(strnum_env_ptr);
    numstr_env_ = std::move(numstr_env_ptr);

    return mgpp::err::make_ok();
}

}} // namespace mmbkpp::app

#endif // !MMUPP_APP_LMDB_BASED_ID_MAPPER_HPP_INCLUDED
