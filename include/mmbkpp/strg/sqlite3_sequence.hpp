
#ifndef MMBKPP_STRG_SQLITE3_SEQUENCE_HPP_INCLUDED
#define MMBKPP_STRG_SQLITE3_SEQUENCE_HPP_INCLUDED

#include <mego/util/std/time.h>
#include <mego/util/get_temp_path.h>
#include <mego/fs/dir.h>

#include "sqlite3_hdl.hpp"

#include <set>
#include <map>
#include <mutex>

#include <fmt/format.h>
#include <fmt/printf.h>
#include <memepp/convert/std/wstring.hpp>
#include <memepp/convert/std/string.hpp>
#include <memepp/convert/fmt.hpp>
#include <memepp/convert/common.hpp>
#include <memepp/string_view.hpp>
#include <memepp/split/self.hpp>
#include <memepp/native.hpp>
#include <mmutilspp/fs/program_path.hpp>
#include <ghc/filesystem.hpp>

namespace outcome = OUTCOME_V2_NAMESPACE;

namespace mmbkpp {
namespace strg { 

struct sqlite3_sequence : public std::enable_shared_from_this<sqlite3_sequence>
{
    using count_t = mmint_t;
    using node_id_t  = mmint_t;
    using index_id_t = mmint_t;

    using open_after_create_table_cb_t = 
        std::function<void(const sqlite3_hdl_sptr&, const memepp::string&, index_id_t, node_id_t)>;

    enum class hdl_status_t 
    {
        ok,
        opening,
        busy,
        unavailabled,
        wait_for_close
    };

    enum class dbfile_status_t 
    {
        ok,
        wait_for_move,
        wait_for_remove,
    };

    enum class sort_t 
    {    
        time_asc,
        time_desc,
    };

    enum class old_action_t
    {
        none,
        remove_old,
        move_old,
    };

    sqlite3_sequence();

    memepp::string filename(node_id_t _node) const;
    memepp::string filepath(index_id_t _index, node_id_t _node) const;

    memepp::string dir_path() const;
    memepp::string file_prefix() const;
    memepp::string file_suffix() const;
    memepp::string table_name() const;

    // online mode
    mgpp::err set_dir_path(const memepp::string& _path);
    mgpp::err set_dir_path(const memepp::string& _path, old_action_t _action);

    mgpp::err set_dir_path_and_move(const memepp::string& _path);
    mgpp::err set_dir_path_and_nothing(const memepp::string& _path);

    mgpp::err copy_all_to_path(const memepp::string& _path);

    void set_file_prefix(const memepp::string& _prefix);
    void set_file_suffix(const memepp::string& _suffix);
    void set_table_name (const memepp::string& _name);
    void set_open_after_create_table_cb(const open_after_create_table_cb_t& _cb);
    void set_max_kb(mmint_t _max_kb);

    outcome::checked<sqlite3_hdl_sptr, mgpp::err>
        get_rw_hdl(index_id_t _index, node_id_t _node, bool _create_if_not_exist = true);
    outcome::checked<sqlite3_hdl_sptr, mgpp::err>
        get_rw_hdl_wait_for(
            index_id_t _index, node_id_t _node, 
            std::chrono::milliseconds _ms, bool _create_if_not_exist = true);
    outcome::checked<sqlite3_hdl_sptr, mgpp::err>
        get_rw_hdl_and_retry(
            index_id_t _index, node_id_t _node, 
            mmint_t _try_count, bool _create_if_not_exist = true);
    outcome::checked<sqlite3_hdl_sptr, mgpp::err>
        get_ro_hdl(index_id_t _index, node_id_t _node);
    outcome::checked<sqlite3_hdl_sptr, mgpp::err>
        get_ro_hdl_wait_for(index_id_t _index, node_id_t _node, std::chrono::milliseconds _ms);
    outcome::checked<sqlite3_hdl_sptr, mgpp::err>
        get_ro_hdl_and_retry(index_id_t _index, node_id_t _node, mmint_t _try_count);

    outcome::checked<count_t, mgpp::err> 
        try_clean_dir_to_limit(sort_t _sort = sort_t::time_asc);

    outcome::checked<count_t, mgpp::err> 
        try_clean_dir_by_removing_out_of_range(
            node_id_t _less_than_node, node_id_t _more_than_node = MMINT_MAX);

    outcome::checked<count_t, mgpp::err> try_close_idle_hdl();

    static mgpp::err global_init();

    static memepp::string make_filename(
        const memepp::string& _file_prefix,
        const memepp::string& _file_suffix,
        node_id_t _node);
    static memepp::string make_filepath(
        const memepp::string& _dir_path,
        const memepp::string& _file_prefix,
        const memepp::string& _file_suffix,
        index_id_t _index, node_id_t _node);

    static void on_close_hdl(const std::shared_ptr<void>& _userdata);
private:

    outcome::checked<sqlite3_hdl_sptr, mgpp::err>
        get_hdl(index_id_t _index, node_id_t _node, bool _is_readonly, bool _create_if_not_exist);

    struct __hdl_onclose_data
    {
        __hdl_onclose_data()
            : index_id_(-1)
            , node_id_(-1)
            , is_readonly_(false)
        {}

        index_id_t index_id_;
        node_id_t  node_id_;
        bool is_readonly_;
        std::weak_ptr<sqlite3_sequence> seq_;
    };

    struct __node_info 
    {
        __node_info()
            : index_id_(-1)
            , node_id_(-1)
            , last_access_ts_(mgu_timestamp_get())
            , dbfile_status_(dbfile_status_t::ok)
            , hdl_status_(hdl_status_t::ok)
            //, old_action_(old_action_t::none)
        {}

        inline constexpr index_id_t index_id__st() const noexcept { return index_id_; }
        inline constexpr node_id_t node_id__st() const noexcept { return node_id_; }

        inline constexpr mgu_timestamp_t last_access_ts__st() const noexcept { return last_access_ts_; }
        
        inline constexpr dbfile_status_t  db_file_status__st() const noexcept { return dbfile_status_; }
        inline constexpr hdl_status_t hdl_status__st() const noexcept { return hdl_status_; }

        inline bool has_ro_hdl_ref__st() const 
        {
            auto count = s_ro_hdl_.use_count();
            if (count)
                return count > 1;
            else
                return !w_ro_hdl_.expired();
        }
        
        inline bool has_rw_hdl_ref__st() const
        {
            auto count = s_rw_hdl_.use_count();
            if (count)
                return count > 1;
            else
                return !w_rw_hdl_.expired();
        }
        
        inline bool has_hdl_ref__st() const
        {
            return has_ro_hdl_ref__st() || has_rw_hdl_ref__st();
        }
        
        inline bool has_internal_hdl__st() const
        {
            return s_ro_hdl_ || s_rw_hdl_;
        }

        inline memepp::string filename__st() const
        {
            return sqlite3_sequence::make_filename(file_prefix_, file_suffix_, node_id_);
        }

        inline memepp::string filepath__st() const
        {
            return sqlite3_sequence::make_filepath(dir_path_, file_prefix_, file_suffix_, index_id_, node_id_);
        }

        mgpp::err try_remove();
        mgpp::err try_move_to(const memepp::string& _filepath);
        mgpp::err try_copy_to(const memepp::string& _filepath);
        

        inline void set_hdl__st(bool _readonly, const sqlite3_hdl_sptr& _hdl)
        {
            if (_readonly) {
                s_ro_hdl_ = _hdl;
                w_ro_hdl_ = _hdl;
            }
            else {
                s_rw_hdl_ = _hdl;
                w_rw_hdl_ = _hdl;
            }
        }

        mutable std::mutex mtx_;
        index_id_t index_id_;
        node_id_t  node_id_;
        mgu_timestamp_t last_access_ts_;
        dbfile_status_t dbfile_status_;
        hdl_status_t hdl_status_;
        //old_action_t old_action_;

        sqlite3_hdl_sptr s_rw_hdl_;
        sqlite3_hdl_sptr s_ro_hdl_;
        std::weak_ptr<sqlite3_hdl> w_rw_hdl_;
        std::weak_ptr<sqlite3_hdl> w_ro_hdl_;
        
        memepp::string dir_path_;
        memepp::string file_prefix_;
        memepp::string file_suffix_;
        
        std::vector<std::tuple<mgu_timestamp_t, memepp::string>> copy_to_list_;
    };
    using __node_info_sptr = std::shared_ptr<__node_info>;

    struct __index_info
    {
        mutable std::mutex mtx_;
        std::map<node_id_t, __node_info_sptr> nodes_;
    };
    using __index_info_sptr = std::shared_ptr<__index_info>;

    mutable std::mutex mtx_;
    mmint_t max_kb_;
    mmint_t max_idle_second_;
    //hdl_status_t all_hdl_status_;

    memepp::string dir_path_;
    memepp::string file_prefix_;
    memepp::string file_suffix_;
    memepp::string table_name_;
    
    std::shared_ptr<open_after_create_table_cb_t> open_after_create_table_cb_;

    std::map<index_id_t, __index_info_sptr> index_infos_;
    std::vector<__node_info_sptr> old_nodes_;
};

inline mgpp::err sqlite3_sequence::__node_info::try_remove()
{
    if (has_hdl_ref__st())
        return mgpp::err{ MGEC__BUSY };

    memepp::string src_path;
    do {
        sqlite3_hdl_sptr ro_hdl;
        sqlite3_hdl_sptr rw_hdl;
        std::lock_guard<std::mutex> locker(mtx_);
        ro_hdl.swap(s_ro_hdl_);
        rw_hdl.swap(s_rw_hdl_);
        src_path = filepath__st();
    } while (0);
    
    auto native_src_path = mm_to<memepp::native_string>(src_path);
    std::error_code ecode;
    if (ghc::filesystem::exists(native_src_path, ecode) && !ecode)
    {
        ghc::filesystem::remove(native_src_path, ecode);
        //if (ecode)
        //    return mgpp::err{ mgec__from_sys_err(ecode.value()) };
    }
    
    return mgpp::err{ mgec__from_sys_err(ecode.value()) };
}

inline mgpp::err sqlite3_sequence::__node_info::try_move_to(const memepp::string& _filepath)
{
    if (has_hdl_ref__st())
        return mgpp::err{ MGEC__BUSY };

    memepp::string src_path;
    do {
        sqlite3_hdl_sptr ro_hdl;
        sqlite3_hdl_sptr rw_hdl;
        std::lock_guard<std::mutex> locker(mtx_);
        ro_hdl.swap(s_ro_hdl_);
        rw_hdl.swap(s_rw_hdl_);
        src_path = filepath__st();
    } while (0);

    auto native_src_path = mm_to<memepp::native_string>(src_path);
    std::error_code ecode;
    if (ghc::filesystem::exists(native_src_path) && !ecode)
    {
        ghc::filesystem::rename(native_src_path, mm_to<memepp::native_string>(_filepath), ecode);
        //if (ecode)
        //    return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    }
    return mgpp::err{ mgec__from_sys_err(ecode.value()) };
}

inline mgpp::err sqlite3_sequence::__node_info::try_copy_to(const memepp::string& _filepath)
{
    if (has_hdl_ref__st())
        return mgpp::err{ MGEC__BUSY };

    memepp::string src_path;
    do {
        sqlite3_hdl_sptr ro_hdl;
        sqlite3_hdl_sptr rw_hdl;
        std::lock_guard<std::mutex> locker(mtx_);
        ro_hdl.swap(s_ro_hdl_);
        rw_hdl.swap(s_rw_hdl_);
        src_path = filepath__st();
    } while (0);

    std::error_code ecode;
    ghc::filesystem::copy(mm_to<memepp::native_string>(src_path), mm_to<memepp::native_string>(_filepath), ecode);
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    return mgpp::err{ MGEC__OK };
}

sqlite3_sequence::sqlite3_sequence()
    : max_kb_(3 * 1024 * 1024)
    , max_idle_second_(600)
    //, all_hdl_status_(hdl_status_t::ok)
    , dir_path_(mmupp::fs::relative_with_program_path("db_seqs"))
    , file_prefix_("node")
    , file_suffix_("db")
    , table_name_ ("data")
{}

inline memepp::string sqlite3_sequence::filename(node_id_t _node) const
{
    std::lock_guard<std::mutex> locker(mtx_);
    return make_filename(file_prefix_, file_suffix_, _node);
}

inline memepp::string sqlite3_sequence::filepath(index_id_t _index, node_id_t _node) const
{
    std::lock_guard<std::mutex> locker(mtx_);
    return make_filepath(dir_path_, file_prefix_, file_suffix_, _index, _node);
}

inline memepp::string sqlite3_sequence::dir_path() const
{
    std::lock_guard<std::mutex> locker(mtx_);
    return dir_path_;
}

inline memepp::string sqlite3_sequence::file_prefix() const
{
    std::lock_guard<std::mutex> locker(mtx_);
    return file_prefix_;
}

inline memepp::string sqlite3_sequence::file_suffix() const
{
    std::lock_guard<std::mutex> locker(mtx_);
    return file_suffix_;
}

inline memepp::string sqlite3_sequence::table_name() const
{
    std::lock_guard<std::mutex> locker(mtx_);
    return table_name_;
}

inline mgpp::err sqlite3_sequence::set_dir_path(const memepp::string& _path)
{
    return set_dir_path_and_nothing(_path);
}

inline mgpp::err sqlite3_sequence::set_dir_path(const memepp::string& _path, old_action_t _action)
{
    std::error_code ecode;
    if (ghc::filesystem::is_regular_file(mm_to<memepp::native_string>(_path), ecode))
        return mgpp::err{ MGEC__ERR, "path is a file" };
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    switch (_action) {
    case old_action_t::move_old: {
        return set_dir_path_and_move(_path);
    }
    default:
    {
        return set_dir_path_and_nothing(_path); 
    }
    }
    //if (!_move_olds) {
    //    if (index_infos_.empty()) {
    //        dir_path_ = _path;
    //        return mgpp::err::make_ok();
    //    }
    //}

    return mgpp::err::make_ok();
}


inline mgpp::err sqlite3_sequence::set_dir_path_and_move(const memepp::string& _path)
{
    auto dst_dirpath = ghc::filesystem::path{ mm_to<memepp::native_string>(_path) };
    auto src_dirpath = mm_to<memepp::native_string>(dir_path());
    if (src_dirpath == dst_dirpath)
        return mgpp::err::make_ok();

    std::error_code ecode;
    if (ghc::filesystem::is_regular_file(dst_dirpath, ecode))
        return mgpp::err{ MGEC__ERR, "path is a file" };
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    auto ec = mgfs__check_and_create_dirs_if_needed(
        _path.data(), _path.size(), 1, 0);
    if (ec) {
        return mgpp::err{ ec, "create dirs failed" };
    }

    std::unique_lock<std::mutex> locker(mtx_);
    dir_path_ = _path;
    locker.unlock();

    if (!ghc::filesystem::is_directory(src_dirpath, ecode))
        return mgpp::err::make_ok();
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    auto dir_iter = ghc::filesystem::directory_iterator(src_dirpath, ecode);
    auto dir_end  = ghc::filesystem::directory_iterator();
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    for (; dir_iter != dir_end; ++dir_iter)
    {
        if (!ghc::filesystem::is_directory(dir_iter->status(ecode)))
            continue;
        if (ecode) {
            continue;
        }

        auto index_name = dir_iter->path().filename().string();
        auto index_iter = ghc::filesystem::directory_iterator(dir_iter->path(), ecode);
        auto index_end  = ghc::filesystem::directory_iterator();
        auto index_id   = atoll(index_name.data());
        if (ecode)
            continue;

        auto dst_index_path = fmt::format("{}/{}", _path, index_name);
        ec = mgfs__check_and_create_dirs_if_needed(
            dst_index_path.data(), dst_index_path.size(), 1, 0);
        if (ec) {
            return mgpp::err{ ec, "create dirs failed" };
        }

        __index_info_sptr index_info;
        locker.lock();
        auto iit = index_infos_.find(index_id);
        if (iit != index_infos_.end())
            index_info = iit->second;
        locker.unlock();
        if (!index_info) {
            for (; index_iter != index_end; ++index_iter)
            {
                if (index_iter->path().extension().string() != file_suffix_.c_str())
                    continue;

                if (!ghc::filesystem::exists(index_iter->path()))
                    continue;

                //std::error_code ecode;
                ghc::filesystem::rename(
                    index_iter->path(),
                    fmt::format("{}/{}", dst_index_path, index_iter->path().filename().string()), ecode);
                //if (!ec) {
                //    ghc::filesystem::remove(index_iter->path(), ec);
                //}
            }
            continue;
        }

        for (; index_iter != index_end; ++index_iter)
        {
            if (!ghc::filesystem::is_regular_file(index_iter->status(ecode)))
                continue;

            if (ecode) {
                continue;
            }

            auto node_name = index_iter->path().filename().string();

            std::vector<memepp::string_view> node_name_parts;
            memepp::split(memepp::view(node_name), ".",
                memepp::split_behavior_t::skip_empty_parts, std::back_inserter(node_name_parts));

            if (node_name_parts.size() != 3)
                continue;

            auto node_id = atoll(node_name_parts[1].to_string().data());

            __node_info_sptr node_info;
            std::unique_lock<std::mutex> index_locker(index_info->mtx_);
            auto nit = index_info->nodes_.find(node_id);
            if (nit != index_info->nodes_.end())
                node_info = nit->second;
            index_locker.unlock();
            
            if (!node_info) {
                //std::error_code ecode;
                ghc::filesystem::rename(
                    index_iter->path(),
                    fmt::format("{}/{}", dst_index_path, node_name), ecode);
                //if (!ec) {
                //    ghc::filesystem::remove(index_iter->path(), ec);
                //}
                continue;
            }

            std::unique_lock<std::mutex> node_locker(node_info->mtx_);
            auto node_filepath = node_info->filepath__st();
            if (index_iter->path() != ghc::filesystem::path{ mm_to<memepp::native_string>(node_filepath) })
            {
                node_locker.unlock();
                continue;
            }
            
            if (node_info->has_hdl_ref__st()) {
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
                
                node_info->dbfile_status_ = dbfile_status_t::wait_for_move;
                node_info->hdl_status_ = hdl_status_t::busy;
                node_locker.unlock();
                
                locker.lock();
                old_nodes_.emplace_back(node_info);
                locker.unlock();

                index_locker.lock();
                index_info->nodes_.erase(node_id);
                index_locker.unlock();
                continue;
            }
            
            do {
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
                node_locker.unlock();
            } while (0);
            
            index_locker.lock();
            index_info->nodes_.erase(node_id);
            index_locker.unlock();

            if (ghc::filesystem::exists(index_iter->path(), ecode) && !ecode)
            {
                //std::error_code ecode;
                ghc::filesystem::rename(
                    index_iter->path(),
                    fmt::format("{}/{}", dst_index_path, node_name), ecode);
            }
            //if (!ec) {
            //    ghc::filesystem::remove(index_iter->path(), ec);
            //}
        }
    }

    return mgpp::err::make_ok();
}

inline mgpp::err sqlite3_sequence::set_dir_path_and_nothing(const memepp::string& _path)
{
    if (dir_path() == _path)
        return mgpp::err::make_ok();

    std::error_code ecode;
    if (ghc::filesystem::is_regular_file(mm_to<memepp::native_string>(_path), ecode))
        return mgpp::err{ MGEC__ERR, "path is a file" };
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    std::unique_lock<std::mutex> locker(mtx_);
    if (index_infos_.empty()) {
        dir_path_ = _path;
        return mgpp::err::make_ok();
    }

    index_id_t curr_index_id = index_infos_.begin()->first;
    dir_path_ = _path;
    locker.unlock();

    for (bool is_iloop = true; is_iloop; ) 
    {
        index_id_t next_index_id;
        MEGOPP_UTIL__ON_SCOPE_CLEANUP([&]
        {
            curr_index_id = next_index_id;
        });

        locker.lock();
        auto iit = index_infos_.find(curr_index_id);
        if (iit == index_infos_.end())
        {
            locker.unlock();
            break;
        }
        auto index_info = iit->second;
        iit = std::next(iit);
        if (iit == index_infos_.end()) {
            is_iloop = false;
            next_index_id = -1;
        }
        else {
            next_index_id = iit->first;
        }
        locker.unlock();

        std::unique_lock<std::mutex> index_locker(index_info->mtx_);
        if (index_info->nodes_.empty())
        {
            index_locker.unlock();
            locker.lock();
            index_infos_.erase(curr_index_id);
            locker.unlock();
            continue;
        }

        auto curr_node_id = index_info->nodes_.begin()->first;
        index_locker.unlock();

        for (bool is_nloop = true; is_nloop;)
        {
            node_id_t next_node_id;
            MEGOPP_UTIL__ON_SCOPE_CLEANUP([&]
            {
                curr_node_id = next_node_id;
            });

            index_locker.lock();
            auto nit = index_info->nodes_.find(curr_node_id);
            if (nit == index_info->nodes_.end())
            {
                index_locker.unlock();
                break;
            }
            auto node_info = nit->second;
            nit = std::next(nit);
            if (nit == index_info->nodes_.end()) 
            {
                is_nloop = false;
                next_node_id = -1;
            }
            else {
                next_node_id = nit->first;
            }
            
            index_info->nodes_.erase(curr_node_id);
            index_locker.unlock();
            
        }
    }
    
    return mgpp::err::make_ok();
}

inline mgpp::err sqlite3_sequence::copy_all_to_path(const memepp::string& _path)
{
    auto dst_dirpath = ghc::filesystem::path{ mm_to<memepp::native_string>(_path) };
    auto src_dirpath = mm_to<memepp::native_string>(dir_path());
    if (src_dirpath == dst_dirpath)
        return mgpp::err::make_ok();

    std::error_code ecode;
    if (ghc::filesystem::is_regular_file(dst_dirpath, ecode))
        return mgpp::err{ MGEC__ERR, "path is a file" };
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    auto ec = mgfs__check_and_create_dirs_if_needed(
        _path.data(), _path.size(), 1, 0);
    if (ec) {
        return mgpp::err{ ec, "create dirs failed" };
    }

    std::unique_lock<std::mutex> locker(mtx_);
    //dir_path_ = _path;
    locker.unlock();

    if (!ghc::filesystem::is_directory(src_dirpath, ecode))
        return mgpp::err::make_ok();
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    auto dir_iter = ghc::filesystem::directory_iterator(src_dirpath, ecode);
    auto dir_end  = ghc::filesystem::directory_iterator();
    if (ecode)
        return mgpp::err{ mgec__from_sys_err(ecode.value()) };

    for (; dir_iter != dir_end; ++dir_iter)
    {
        if (!ghc::filesystem::is_directory(dir_iter->status(ecode)))
            continue;
        if (ecode) {
            continue;
        }

        auto index_name = dir_iter->path().filename().string();
        auto index_iter = ghc::filesystem::directory_iterator(dir_iter->path(), ecode);
        auto index_end  = ghc::filesystem::directory_iterator();
        auto index_id   = atoll(index_name.data());
        if (ecode)
            return mgpp::err{ mgec__from_sys_err(ecode.value()) };

        auto dst_index_path = fmt::format("{}/{}", _path, index_name);
        ec = mgfs__check_and_create_dirs_if_needed(
            dst_index_path.data(), dst_index_path.size(), 1, 0);
        if (ec) {
            return mgpp::err{ ec, "create dirs failed" };
        }

        __index_info_sptr index_info;
        locker.lock();
        auto iit = index_infos_.find(index_id);
        if (iit != index_infos_.end())
            index_info = iit->second;
        locker.unlock();
        if (!index_info) {
            for (; index_iter != index_end; ++index_iter)
            {
                if (index_iter->path().extension().string() != file_suffix_.c_str())
                    continue;
                
                if (!ghc::filesystem::exists(index_iter->path(), ecode))
                    continue;
                if (ecode) {
                    continue;
                }

                //std::error_code ecode;
                ghc::filesystem::copy_file(
                    index_iter->path(),
                    fmt::format("{}/{}", dst_index_path, index_iter->path().filename().string()), 
                    ecode
                );
            }
            continue;
        }

        for (; index_iter != index_end; ++index_iter)
        {
            if (!ghc::filesystem::is_regular_file(index_iter->status(ecode)))
                continue;
            if (ecode) {
                continue;
            }

            auto node_name = index_iter->path().filename().string();

            std::vector<memepp::string_view> node_name_parts;
            memepp::split(memepp::view(node_name), ".",
                memepp::split_behavior_t::skip_empty_parts, std::back_inserter(node_name_parts));

            if (node_name_parts.size() != 3)
                continue;

            auto node_id = atoll(node_name_parts[1].to_string().data());

            __node_info_sptr node_info;
            std::unique_lock<std::mutex> index_locker(index_info->mtx_);
            auto nit = index_info->nodes_.find(node_id);
            if (nit != index_info->nodes_.end())
                node_info = nit->second;
            index_locker.unlock();

            if (!node_info) {
                //std::error_code ecode;
                ghc::filesystem::copy_file(
                    index_iter->path(),
                    fmt::format("{}/{}", dst_index_path, index_iter->path().filename().string()), 
                    ecode
                );
                continue;
            }

            std::unique_lock<std::mutex> node_locker(node_info->mtx_);
            auto node_filepath = node_info->filepath__st();

            if (node_info->has_hdl_ref__st())
            {
                // TO_DO
                
                node_info->hdl_status_ = hdl_status_t::busy;
                
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
                node_locker.unlock();
                continue;
            }

            do {
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
                node_locker.unlock();
            } while (0);

            //std::error_code ecode;
            ghc::filesystem::copy_file(
                index_iter->path(),
                fmt::format("{}/{}", dst_index_path, node_name),
                ecode);
        }
    }

    return mgpp::err::make_ok();
}

inline void sqlite3_sequence::set_file_prefix(const memepp::string& _prefix)
{
    std::lock_guard<std::mutex> locker(mtx_);
    if (index_infos_.empty()) {
        file_prefix_ = _prefix;
    }
}

inline void sqlite3_sequence::set_file_suffix(const memepp::string& _suffix)
{
    std::lock_guard<std::mutex> locker(mtx_);
    if (index_infos_.empty()) {
        file_suffix_ = _suffix;
    }
}

inline void sqlite3_sequence::set_open_after_create_table_cb(
    const open_after_create_table_cb_t& _cb)
{
    std::lock_guard<std::mutex> locker(mtx_);
    open_after_create_table_cb_ = std::make_shared<open_after_create_table_cb_t>(_cb);
}

inline void sqlite3_sequence::set_max_kb(mmint_t _max_kb)
{
    std::lock_guard<std::mutex> locker(mtx_);
    max_kb_ = _max_kb;
}

inline void sqlite3_sequence::set_table_name(const memepp::string& _name)
{
    std::lock_guard<std::mutex> locker(mtx_);
    if (index_infos_.empty()) {
        table_name_ = _name;
    }
}

inline outcome::checked<sqlite3_hdl_sptr, mgpp::err>
    sqlite3_sequence::get_rw_hdl(index_id_t _index, node_id_t _node, bool _create_if_not_exist)
{
    return get_hdl(_index, _node, false, _create_if_not_exist);
}

outcome::checked<sqlite3_hdl_sptr, mgpp::err>
    sqlite3_sequence::get_rw_hdl_wait_for(
        index_id_t _index, node_id_t _node,
        std::chrono::milliseconds _ms, bool _create_if_not_exist)
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    mgpp::err err;
    do {
        auto result = get_rw_hdl(_index, _node, _create_if_not_exist);
        if (result) {
            return result;
        }
        
        std::this_thread::yield();
        err = result.error();
    } while (std::chrono::steady_clock::now() - start < _ms);
    
    return outcome::failure(err);
}

outcome::checked<sqlite3_hdl_sptr, mgpp::err>
    sqlite3_sequence::get_rw_hdl_and_retry(
        index_id_t _index, node_id_t _node,
        mmint_t _try_count, bool _create_if_not_exist)
{
    mgpp::err err;
    do {
        auto result = get_rw_hdl(_index, _node, _create_if_not_exist);
        if (result) {
            return result;
        }
        std::this_thread::yield();
        err = result.error();
    } while (_try_count-- > 0);
    return outcome::failure(err);
}

inline outcome::checked<sqlite3_hdl_sptr, mgpp::err>
    sqlite3_sequence::get_ro_hdl(index_id_t _index, node_id_t _node)
{
    return get_hdl(_index, _node, true, false);
}

outcome::checked<sqlite3_hdl_sptr, mgpp::err>
    sqlite3_sequence::get_ro_hdl_wait_for(
        index_id_t _index, node_id_t _node, std::chrono::milliseconds _ms)
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    mgpp::err err;
    do {
        auto result = get_ro_hdl(_index, _node);
        if (result) {
            return result;
        }
            
        std::this_thread::yield();
        err = result.error();
        if (result.error().code() == MGEC__NOENT)
            break;
    } while (std::chrono::steady_clock::now() - start < _ms);
    return outcome::failure(err);
}

outcome::checked<sqlite3_hdl_sptr, mgpp::err>
    sqlite3_sequence::get_ro_hdl_and_retry(
        index_id_t _index, node_id_t _node, mmint_t _try_count)
{
    mgpp::err err;
    do {
        auto result = get_ro_hdl(_index, _node);
        if (result) {
            return result;
        }
        std::this_thread::yield();
        err = result.error();
    } while (_try_count-- > 0);
    return outcome::failure(err);
}

inline outcome::checked<sqlite3_hdl_sptr, mgpp::err>
        sqlite3_sequence::get_hdl(index_id_t _index, node_id_t _node, bool _is_readonly, bool _create_if_not_exist)
{

    std::error_code ecode;
    std::unique_lock<std::mutex> locker(mtx_);
    __index_info_sptr index_info;
    auto iit = index_infos_.find(_index);
    if (iit == index_infos_.end()) {
        
        auto ec = mgfs__check_and_create_dirs_if_needed(dir_path_.c_str(), dir_path_.size(), 1, 0);
        if (ec != 0) {
            return outcome::failure(mgpp::err{ 
                ec, _create_if_not_exist ? "failed to create directory": "directory not exists" });
        }
        
        auto index_path = fmt::format("{}/{:0>16}", dir_path_, _index);
        
        ec = mgfs__check_and_create_dirs_if_needed(
            index_path.c_str(), index_path.size(), _create_if_not_exist, 1);
        if (ec != 0) {
            return outcome::failure(mgpp::err{ 
                ec, _create_if_not_exist ? "failed to create directory": "directory not exists" });
        }

        if (!_create_if_not_exist) {
            auto node_u8path = make_filepath(dir_path_, file_prefix_, file_suffix_, _index, _node);
            if (!ghc::filesystem::exists(mm_to<memepp::native_string>(node_u8path), ecode))
                return outcome::failure(mgpp::err{ MGEC__NOENT, "index not exists" });
            if (ecode) {
                return outcome::failure(mgpp::err{ mgec__from_sys_err(ecode.value()) });
            }

        }
        iit = index_infos_.emplace(_index, std::make_shared<__index_info>()).first;
    }
    index_info = iit->second;
    locker.unlock();

    std::unique_lock<std::mutex> index_locker(index_info->mtx_);
    __node_info_sptr node_info;
    bool has_create_node = false;
    auto nit = index_info->nodes_.find(_node);
    if (nit == index_info->nodes_.end()) {

        auto node_u8path = filepath(_index, _node);
        if (!ghc::filesystem::exists(mm_to<memepp::native_string>(node_u8path), ecode) && !_create_if_not_exist)
        {
            return outcome::failure(mgpp::err{ MGEC__NOENT, "node not exists" });
        }
        if (ecode) {
            return outcome::failure(mgpp::err{ mgec__from_sys_err(ecode.value()) });
        }

        auto ninfo = std::make_shared<__node_info>();
        ninfo->index_id_ = _index;
        ninfo->node_id_  = _node;
        
        ninfo->dir_path_    = dir_path();
        ninfo->file_prefix_ = file_prefix();
        ninfo->file_suffix_ = file_suffix();
        
        nit = index_info->nodes_.emplace(_node, ninfo).first;
        has_create_node = true;
    }
    node_info = nit->second;
    index_locker.unlock();

    std::unique_lock<std::mutex> node_locker(node_info->mtx_);
    //if (node_info->db_file_status_ == dbfile_status_t::wait_for_move)
    //{
    //    if (has_create_node) {
    //        index_locker.lock();
    //        index_info->nodes_.erase(_node);
    //        index_locker.unlock();
    //    }
    //    
    //}

    if (node_info->db_file_status__st() == dbfile_status_t::wait_for_remove)
    {
        node_locker.unlock();
        
        if (has_create_node) {
            index_locker.lock();
            index_info->nodes_.erase(_node);
            index_locker.unlock();
        }

        auto err = node_info->try_remove();
        if (err) {
            return outcome::failure(err);
        }

        return outcome::failure(mgpp::err{ MGEC__BUSY, "database is wait for delete" });
    }

    if (node_info->hdl_status__st() == hdl_status_t::unavailabled)
    {
        return outcome::failure(mgpp::err{ MGEC__AGAIN, "handle is unavailabled" });
    }

    if (node_info->hdl_status__st() == hdl_status_t::busy)
    {
        return outcome::failure(mgpp::err{ MGEC__BUSY, "handle is busy" });
    }

    if (node_info->hdl_status__st() == hdl_status_t::opening)
    {
        return outcome::failure(mgpp::err{ MGEC__AGAIN, "handle is opening" });
    }

    if (node_info->hdl_status__st() == hdl_status_t::wait_for_close)
    {
        if (node_info->has_hdl_ref__st())
        {
            return outcome::failure(mgpp::err{ MGEC__AGAIN, "handle is wait for close" });
        }

        do {
            sqlite3_hdl_sptr ro_hdl;
            sqlite3_hdl_sptr rw_hdl;
            ro_hdl.swap(node_info->s_ro_hdl_);
            rw_hdl.swap(node_info->s_rw_hdl_);
            node_locker.unlock();
        } while (0);

        if (has_create_node) {
            index_locker.lock();
            index_info->nodes_.erase(_node);
            index_locker.unlock();
        }
        return outcome::failure(mgpp::err{ MGEC__AGAIN, "handle is wait for close" });
    }
    
    if (_is_readonly) {
        if (node_info->s_ro_hdl_) {
            node_info->last_access_ts_ = mgu_timestamp_get();
            return outcome::success(node_info->s_ro_hdl_);
        }
    }
    else {
        if (node_info->s_rw_hdl_) {
            node_info->last_access_ts_ = mgu_timestamp_get();
            return outcome::success(node_info->s_rw_hdl_);
        }
    }
    
    node_info->hdl_status_ = hdl_status_t::opening;
    node_locker.unlock();

    int create_flags;
    if (_is_readonly) {
        create_flags = SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX;
    }
    else {
        create_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    }
    auto node_u8path = filepath(_index, _node);
    auto hdl_ret = sqlite3_hdl::open_to_shared(node_u8path.data(), create_flags);
    if (!hdl_ret) {
        node_locker.lock();
        bool has_ex_hdl = (node_info->has_hdl_ref__st());
        node_info->hdl_status_ = hdl_status_t::unavailabled;
        node_locker.unlock();
        if (has_create_node && !has_ex_hdl)
        {
            index_locker.lock();
            index_info->nodes_.erase(_node);
            index_locker.unlock();
        }
        
        return outcome::failure(hdl_ret.error());
    }

    locker.lock();
    auto open_after_create_table_cb = open_after_create_table_cb_;
    auto table_name = table_name_;
    locker.unlock();

    try {
        if (!_is_readonly && open_after_create_table_cb)
            (*open_after_create_table_cb)(hdl_ret.value(), table_name, _index, _node);
    }
    catch (std::exception& ex) {
        return outcome::failure(mgpp::err{ MGEC__ERR, ex.what() });
    }
    catch (...) {
        return outcome::failure(mgpp::err{ MGEC__ERR, "unknown exception" });
    }

    auto data  = std::make_shared<__hdl_onclose_data>();
    data->seq_ = shared_from_this();
    data->index_id_ = _index;
    data->node_id_  = _node;
    data->is_readonly_ = _is_readonly;

    hdl_ret.value()->set_userdata(data);
    hdl_ret.value()->set_close_cb(on_close_hdl);

    node_locker.lock();
    node_info->set_hdl__st(_is_readonly, hdl_ret.value());

    node_info->hdl_status_ = hdl_status_t::ok;
    node_info->last_access_ts_ = mgu_timestamp_get();
    node_locker.unlock();

    return outcome::success(hdl_ret.value());
}

outcome::checked<sqlite3_sequence::count_t, mgpp::err> 
    sqlite3_sequence::try_clean_dir_to_limit(sort_t _sort)
{
    auto dir_u8path = dir_path();

    if (mgfs__is_exist_dir(dir_u8path.c_str(), dir_u8path.size()) != 1) 
    {
        return outcome::failure(mgpp::err{ MGEC__NOENT, "dir not exists" });
    }

    mmint_t total_kb = 0;
    size_t total_count = 0;
    std::map<node_id_t, std::set<index_id_t>> all_nodes;
    
    std::error_code ecode;
    auto dir_path = mm_to<memepp::native_string>(dir_u8path);
    auto dir_iter = ghc::filesystem::directory_iterator(dir_path, ecode);
    auto dir_end  = ghc::filesystem::directory_iterator();
    if (ecode) {
        return outcome::failure(mgpp::err{ mgec__from_sys_err(ecode.value()) });
    }

    for (; dir_iter != dir_end; ++dir_iter) 
    {
        if (!ghc::filesystem::is_directory(dir_iter->status(ecode)))
            continue;
        if (ecode) {
            continue;
        }

        auto index_name = dir_iter->path().filename().string();
        auto index_iter = ghc::filesystem::directory_iterator(dir_iter->path(), ecode);
        auto index_end  = ghc::filesystem::directory_iterator();
        if (ecode) {
            continue;
        }
        
        auto index_id = atoll(index_name.data());
        for (; index_iter != index_end; ++index_iter)
        {
            if (!ghc::filesystem::is_regular_file(index_iter->status(ecode)))
                continue;
            if (ecode) {
                continue;
            }

            auto node_name = index_iter->path().filename().string();
            auto node_path = index_iter->path();
            
            if (node_path.extension().string() != file_suffix_.c_str())
                continue;

            if (!ghc::filesystem::exists(index_iter->path(), ecode))
                continue;
            if (ecode) {
                continue;
            }
            
            std::vector<memepp::string_view> node_name_parts;
            memepp::split(memepp::view(node_name), ".", 
                memepp::split_behavior_t::skip_empty_parts, std::back_inserter(node_name_parts));

            if (node_name_parts.size() != 3)
                continue;
            
            auto node_id = atoll(node_name_parts[1].to_string().data());

            all_nodes[node_id].emplace(index_id);
            ++total_count;
            total_kb += (ghc::filesystem::file_size(node_path, ecode) / 1024);
        }
    }

    count_t count = 0;

    if (total_kb <= max_kb_) {
        return outcome::success(count);
    }

    auto avg_kb = double(total_kb) / total_count;
    if (MEGO_SYMBOL__LIKELY(avg_kb == 0))
        return outcome::success(count);

    auto del_count = (total_kb - max_kb_) / avg_kb;
    if (MEGO_SYMBOL__LIKELY(del_count <= 0))
        return outcome::success(count);

    std::map<index_id_t, std::set<node_id_t>> dels;
    if (_sort == sort_t::time_asc) 
    {
        for (auto nit = all_nodes.begin(); nit != all_nodes.end(); ++nit) 
        {
            auto& indexs = nit->second;
            for (auto iit = indexs.begin(); iit != indexs.end(); ++iit)
            {
                if (count >= del_count) 
                    break;
                ++count;
                
                dels[*iit].emplace(nit->first);
            }
        }
    }
    else {
        for (auto nit = all_nodes.rbegin(); nit != all_nodes.rend(); ++nit) 
        {
            auto& indexs = nit->second;
            for (auto iit = indexs.rbegin(); iit != indexs.rend(); ++iit)
            {
                if (count >= del_count) 
                    break;
                
                dels[*iit].emplace(nit->first);
                ++count;
            }
        }
    }

    count = 0;
    for (auto& delIt : dels) 
    {
        auto index_id = delIt.first;
        auto& nodes = delIt.second;

        std::unique_lock<std::mutex> locker(mtx_);
        auto iit = index_infos_.find(index_id);
        if (iit == index_infos_.end()) {
            locker.unlock();
            for (auto node_id : nodes) {
                //std::error_code ecode;
                ghc::filesystem::remove(mm_to<memepp::native_string>(filepath(index_id, node_id)), ecode);
                ++count;
            }
            continue;
        }
        auto index_info = iit->second;
        locker.unlock();

        std::unique_lock<std::mutex> index_locker(index_info->mtx_);
        for (auto node_id : nodes) 
        {
            auto nit = index_info->nodes_.find(node_id);
            if (nit == index_info->nodes_.end()) {
                //std::error_code ecode;
                ghc::filesystem::remove(mm_to<memepp::native_string>(filepath(index_id, node_id)), ecode);
                ++count;
                continue;
            }

            auto node_info = nit->second;
            index_locker.unlock();

            std::unique_lock<std::mutex> node_locker(node_info->mtx_);
            if (node_info->has_hdl_ref__st())
            {
                node_info->dbfile_status_ = dbfile_status_t::wait_for_remove;
                node_info->hdl_status_ = hdl_status_t::unavailabled;
                node_locker.unlock();
                continue;
            }

            auto fpath = node_info->filepath__st();
            do {
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
                node_locker.unlock();
            } while (0);

            //std::error_code ecode;
            auto native_fpath = mm_to<memepp::native_string>(fpath);
            if (ghc::filesystem::exists(native_fpath, ecode) && !ecode)
            {
                ghc::filesystem::remove(native_fpath, ecode);
            }
            ++count;
        }
    }

    return outcome::success(count);
}

outcome::checked<sqlite3_sequence::count_t, mgpp::err> 
    sqlite3_sequence::try_clean_dir_by_removing_out_of_range(
        node_id_t _less_than_node, node_id_t _more_than_node)
{
    auto dir_u8path = dir_path();

    if (mgfs__is_exist_dir(dir_u8path.c_str(), dir_u8path.size()) != 1) 
    {
        return outcome::failure(mgpp::err{ MGEC__NOENT, "dir not exists" });
    }

    std::error_code ecode;
    auto dir_path = mm_to<memepp::native_string>(dir_u8path);
    auto dir_iter = ghc::filesystem::directory_iterator(dir_path, ecode);
    auto dir_end  = ghc::filesystem::directory_iterator();
    if (ecode) {
        return outcome::failure(mgpp::err{ mgec__from_sys_err(ecode.value()) });
    }

    count_t count = 0;
    for (; dir_iter != dir_end; ++dir_iter) 
    {
        if (!ghc::filesystem::is_directory(dir_iter->status(ecode))) 
            continue;
        if (ecode) {
            continue;
        }

        auto index_name = dir_iter->path().filename().string();
        auto index_iter = ghc::filesystem::directory_iterator(dir_iter->path(), ecode);
        auto index_end  = ghc::filesystem::directory_iterator();
        if (ecode) {
            continue;
        }
        
        std::map<node_id_t, std::string> node_paths;
        for (; index_iter != index_end; ++index_iter)
        {
            if (index_iter->path().extension().string() != file_suffix_.c_str())
                continue;

            if (!ghc::filesystem::exists(index_iter->path(), ecode))
                continue;
            if (ecode) {
                continue;
            }

            if (!ghc::filesystem::is_regular_file(index_iter->status(ecode))) 
                continue;
            if (ecode) {
                continue;
            }

            auto node_name = index_iter->path().filename().string();

            std::vector<memepp::string_view> node_name_parts;
            memepp::split(memepp::view(node_name), ".",
                memepp::split_behavior_t::skip_empty_parts, std::back_inserter(node_name_parts));

            if (node_name_parts.size() != 3)
                continue;
            
            auto node_id = atoll(node_name_parts[1].to_string().data());

            if (node_id <= _less_than_node || node_id >= _more_than_node) 
            {
                node_paths.emplace(node_id, index_iter->path().string());
            }
        }

        if (node_paths.empty())
            continue;
        
        auto index_id = atoll(index_name.data());
        
        std::unique_lock<std::mutex> locker(mtx_);

        auto iit = index_infos_.find(index_id);
        if (iit == index_infos_.end()) {
            locker.unlock();
            for (auto path : node_paths) {
                ghc::filesystem::remove(path.second, ecode);
                ++count;
            }
            continue;
        }

        auto index_info = iit->second;
        locker.unlock();

        for (auto path : node_paths)
        {
            std::unique_lock<std::mutex> index_locker(index_info->mtx_);
            auto nit = index_info->nodes_.find(path.first);
            if (nit == index_info->nodes_.end()) {
                index_locker.unlock();
                ghc::filesystem::remove(path.second, ecode);
                ++count;
                continue;
            }

            auto node_info = nit->second;
            index_locker.unlock();

            std::unique_lock<std::mutex> node_locker(node_info->mtx_);
            if (node_info->has_hdl_ref__st())
            {
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
                
                node_info->dbfile_status_ = dbfile_status_t::wait_for_remove;
                node_info->hdl_status_ = hdl_status_t::unavailabled;
                node_locker.unlock();
                continue;
            }

            do {
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
                node_locker.unlock();
            } while (0);
            
            if (ghc::filesystem::exists(path.second, ecode) && !ecode)
            {
                ghc::filesystem::remove(path.second, ecode);
            }
            ++count;
        }
    }

    return outcome::success(count);
}

outcome::checked<sqlite3_sequence::count_t, mgpp::err> sqlite3_sequence::try_close_idle_hdl()
{
    std::unique_lock<std::mutex> locker(mtx_);
    //if (all_hdl_status_ == hdl_status_t::busy) 
    //{
    //    return outcome::success(0);
    //}

    if (index_infos_.empty()) {
        return outcome::success(0);
    }

    //all_hdl_status_ = hdl_status_t::busy;
    auto curr_index_id = index_infos_.begin()->first;
    locker.unlock();

    count_t count = 0;

    for (bool is_iloop = true; is_iloop;) 
    {
        index_id_t next_index_id;
        MEGOPP_UTIL__ON_SCOPE_CLEANUP([&]
        {
            curr_index_id = next_index_id;
        });

        locker.lock();
        auto iit = index_infos_.find(curr_index_id);
        if (iit == index_infos_.end()) 
        {
            //all_hdl_status_ = hdl_status_t::ok;
            locker.unlock();
            break;
        }
        auto index_info = iit->second;
        iit = std::next(iit);
        if (iit == index_infos_.end()) {
            is_iloop = false;
            next_index_id = -1;
        }
        else {
            next_index_id = iit->first;
        }
        locker.unlock();

        std::unique_lock<std::mutex> index_locker(index_info->mtx_);
        if (index_info->nodes_.empty()) 
        {
            index_locker.unlock();
            locker.lock();
            index_infos_.erase(curr_index_id);
            locker.unlock();
            continue;
        }
        
        auto curr_node_id = index_info->nodes_.begin()->first;
        index_locker.unlock();

        for (bool is_nloop = true; is_nloop;) 
        {
            node_id_t next_node_id;
            MEGOPP_UTIL__ON_SCOPE_CLEANUP([&]
            {
                curr_node_id = next_node_id;
            });

            index_locker.lock();
            auto nit = index_info->nodes_.find(curr_node_id);
            if (nit == index_info->nodes_.end()) 
            {
                index_locker.unlock();
                break;
            }
            auto node_info = nit->second;
            nit = std::next(nit);
            if (nit == index_info->nodes_.end()) {
                is_nloop = false;
                next_node_id = -1;
            }
            else {
                next_node_id = nit->first;
            }
            index_locker.unlock();

            std::unique_lock<std::mutex> node_locker(node_info->mtx_);
            if (node_info->s_ro_hdl_.use_count() > 1 || node_info->s_rw_hdl_.use_count() > 1) 
            {
                continue;
            }
            
            if (mgu_timestamp_get() - node_info->last_access_ts__st() < max_idle_second_ * 1000)
            {
                continue;
            }

            sqlite3_hdl_sptr ro_hdl;
            sqlite3_hdl_sptr rw_hdl;
            ro_hdl.swap(node_info->s_ro_hdl_);
            rw_hdl.swap(node_info->s_rw_hdl_);
            node_locker.unlock();
            
            ++count;
        }
    }

    return outcome::success(count);
}

inline mgpp::err sqlite3_sequence::global_init()
{
    if (sqlite3_threadsafe() == 0) {
        return mgpp::err{ MGEC__ERR, "sqlite3 is not threadsafe" };
    }

#if MG_OS__WIN_AVAIL    
    static char __temp_directory[MAX_PATH] = { 0 };
    if (__temp_directory[0] == 0) {
        mgu_get_temp_path(__temp_directory, MAX_PATH);
        strncat(__temp_directory, "/sqlite3_sequence_temp",
            MAX_PATH - strlen(__temp_directory) - 1);
    }

    std::error_code ecode;
    if (!ghc::filesystem::exists(__temp_directory, ecode) && !ecode)
    {
        ghc::filesystem::create_directories(__temp_directory, ecode);
        if (ecode) {
            return mgpp::err{ MGEC__ERR, "failed to create temp directory" };
        }
    }

    //sqlite3_temp_directory = __temp_directory;
#endif
    return mgpp::err::make_ok();
}

inline memepp::string sqlite3_sequence::make_filename(
    const memepp::string& _file_prefix,
    const memepp::string& _file_suffix,
    node_id_t _node)
{
    return mm_from(fmt::format("{}.{:0>16}.{}", _file_prefix, _node, _file_suffix));
}

inline memepp::string sqlite3_sequence::make_filepath(
    const memepp::string& _dir_path,
    const memepp::string& _file_prefix,
    const memepp::string& _file_suffix,
    index_id_t _index, node_id_t _node)
{
    return mm_from(fmt::format("{}/{:0>16}/{}.{:0>16}.{}",
        _dir_path,
        _index,
        _file_prefix,
        _node,
        _file_suffix));
}

inline void sqlite3_sequence::on_close_hdl(const std::shared_ptr<void>& _userdata)
{
    auto data = std::static_pointer_cast<__hdl_onclose_data>(_userdata);
    if (!data) {
        return;
    }

    auto seq = data->seq_.lock();
    if (!seq) {
        return;
    }
    
    auto new_filepath = seq->filepath(data->index_id_, data->node_id_);

    std::unique_lock<std::mutex> locker(seq->mtx_);
    auto iit = seq->index_infos_.find(data->index_id_);
    if (iit == seq->index_infos_.end()) {
        return;
    }

    auto index_info = iit->second;
    locker.unlock();

    std::unique_lock<std::mutex> index_locker(index_info->mtx_);
    auto nit = index_info->nodes_.find(data->node_id_);

    if (nit == index_info->nodes_.end()) {
        return;
    }

    auto node_info = nit->second;
    index_locker.unlock();

    std::unique_lock<std::mutex> node_locker(node_info->mtx_);

    std::error_code ecode;
    // move operation
    auto old_filepath = node_info->filepath__st();
    if (new_filepath != old_filepath)
    {
        node_locker.unlock();
        
        std::vector<__node_info_sptr> old_nodes;
        locker.lock();
        for (auto it = seq->old_nodes_.begin(); it != seq->old_nodes_.end(); )
        {
            if ((*it)->index_id__st() != data->index_id_ ||
                (*it)->node_id__st()  != data->node_id_)
            {
                ++it;
                continue;
            }
            
            old_nodes.push_back(*it);
            it = seq->old_nodes_.erase(it);
        }
        locker.unlock();

        if (old_nodes.empty()) {
            return;
        }

        std::sort(old_nodes.begin(), old_nodes.end(), [](const __node_info_sptr& _lhs, const __node_info_sptr& _rhs) 
        {
            return _lhs->last_access_ts__st() < _rhs->last_access_ts__st();
        });
        
        for (auto& node : old_nodes) 
        {
            std::unique_lock<std::mutex> oldnode_locker(node->mtx_);
            auto src_path = mm_to<memepp::native_string>(node->filepath__st());
            auto copy_to_list = node->copy_to_list_;
            oldnode_locker.unlock();
            if (!ghc::filesystem::is_regular_file(src_path, ecode))
            {
                continue;
            }
            if (ecode) {
                continue;
            }

            auto now_ts = mgu_timestamp_get();
            for (auto& copy_to : copy_to_list)
            {
                if (now_ts - std::get<0>(copy_to) > 30 * 1000)
                {
                    continue;
                }
                
                ghc::filesystem::copy_file(
                    mm_to<memepp::native_string>(old_filepath),
                    mm_to<memepp::native_string>(std::get<1>(copy_to)),
                    ecode);
                if (ecode) {
                    break;
                }
            }
            
            ghc::filesystem::rename(src_path, mm_to<memepp::native_string>(new_filepath), ecode);
            return;
        }
        return;
    }

    if (data->is_readonly_) {
        node_info->s_ro_hdl_.reset();
    }
    else {
        node_info->s_rw_hdl_.reset();
    }
    bool has_hdl = (node_info->has_hdl_ref__st() && node_info->has_internal_hdl__st());
    bool db_remove = (node_info->db_file_status__st() == dbfile_status_t::wait_for_remove);
    auto copy_to_list = node_info->copy_to_list_;
    node_locker.unlock();

    // copy operation
    if (!has_hdl && copy_to_list.size())
    {
        auto now_ts = mgu_timestamp_get();
        for (auto& copy_to : copy_to_list)
        {
            if (now_ts - std::get<0>(copy_to) > 30 * 1000)
            {
                continue;
            }
            
            ghc::filesystem::copy_file(
                mm_to<memepp::native_string>(old_filepath),
                mm_to<memepp::native_string>(std::get<1>(copy_to)),
                ecode);
            if (ecode) {
                break;
            }
        }
        
        if (!db_remove) {
            node_locker.lock();
            node_info->hdl_status_ = hdl_status_t::ok;
            node_locker.unlock();
        }
    }

    // remove operation
    if (!has_hdl && db_remove)
    {
        ghc::filesystem::remove(mm_to<memepp::native_string>(old_filepath), ecode);
        if (ecode) {
            // TODO: log
        }
    }

    if (has_hdl) {
        return;
    }

    if (db_remove) {
        index_locker.lock();
        index_info->nodes_.erase(data->node_id_);
        index_locker.unlock();
    }

}

}
} // namespace mmbkpp

#endif // !MMBKPP_STRG_SQLITE3_SEQUENCE_HPP_INCLUDED
