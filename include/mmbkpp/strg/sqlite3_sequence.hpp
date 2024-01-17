
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
#include <memepp/convert/std/wstring.hpp>
#include <memepp/convert/std/string.hpp>
#include <memepp/convert/fmt.hpp>
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

    using create_table_cb_t = 
        std::function<void(const sqlite3_hdl_sptr&, const memepp::string&, index_id_t, node_id_t)>;

    enum class hdl_status_t 
    {
        ok,
        creating,
        busy,
        unavail,
        wait_for_close
    };

    enum class db_status_t 
    {
        ok,
        moving,
        wait_for_delete,
    };

    enum class sort_t 
    {    
        time_asc,
        time_desc,
    };

    enum class old_action_t
    {
        none,
        delete_old,
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

    void set_file_prefix(const memepp::string& _prefix);
    void set_file_suffix(const memepp::string& _suffix);
    void set_table_name(const memepp::string& _name);
    void set_create_table_cb(const create_table_cb_t& _cb);
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
            , db_status_(db_status_t::ok)
            , hdl_status_(hdl_status_t::ok)
        {}

        inline bool is_ro_hdl_referenced() const 
        {
            auto count = s_ro_hdl_.use_count();
            if (count)
                return count > 1;
            else
                return !w_ro_hdl_.expired();
        }
        
        inline bool is_rw_hdl_referenced() const 
        {
            auto count = s_rw_hdl_.use_count();
            if (count)
                return count > 1;
            else
                return !w_rw_hdl_.expired();
        }
        
        inline bool is_hdl_referenced() const 
        {
            return is_ro_hdl_referenced() || is_rw_hdl_referenced();
        }
        
        inline bool has_hdl() const
        {
            return s_ro_hdl_ || s_rw_hdl_;
        }

        inline void set_hdl(bool _readonly, const sqlite3_hdl_sptr& _hdl)
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
        sqlite3_hdl_sptr s_rw_hdl_;
        std::weak_ptr<sqlite3_hdl> w_rw_hdl_;
        sqlite3_hdl_sptr s_ro_hdl_;
        std::weak_ptr<sqlite3_hdl> w_ro_hdl_;
        mgu_timestamp_t last_access_ts_;
        db_status_t db_status_;
        hdl_status_t hdl_status_;
        
        memepp::string dir_path_;
        memepp::string file_prefix_;
        memepp::string file_suffix_;
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
    hdl_status_t all_hdl_status_;

    memepp::string dir_path_;
    memepp::string file_prefix_;
    memepp::string file_suffix_;
    memepp::string table_name_;
    
    std::shared_ptr<create_table_cb_t> create_table_cb_;

    std::map<index_id_t, __index_info_sptr> index_infos_;
};

sqlite3_sequence::sqlite3_sequence()
    : max_kb_(3 * 1024 * 1024)
    , max_idle_second_(600)
    , all_hdl_status_(hdl_status_t::ok)
    , dir_path_(mmupp::fs::relative_with_program_path("dbs"))
    , file_prefix_("index")
    , file_suffix_("db")
    , table_name_ ("data")
{}

inline memepp::string sqlite3_sequence::filename(node_id_t _node) const
{
    std::lock_guard<std::mutex> locker(mtx_);
    return mm_from(fmt::format("{}.{:0>16}.{}", file_prefix_, _node, file_suffix_));
}

inline memepp::string sqlite3_sequence::filepath(index_id_t _index, node_id_t _node) const
{
    return mm_from(fmt::format("{}/{:0>16}/{}", dir_path(), _index, filename(_node)));
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
    return set_dir_path(_path, old_action_t::none);
}

inline mgpp::err sqlite3_sequence::set_dir_path(const memepp::string& _path, old_action_t _action)
{
    std::lock_guard<std::mutex> locker(mtx_);
    if (ghc::filesystem::is_regular_file(mm_to<memepp::native_string>(_path)))
        return mgpp::err{ MGEC__ERR, "path is a file" };

    //if (!_move_olds) {
    //    if (index_infos_.empty()) {
    //        dir_path_ = _path;
    //        return mgpp::err::make_ok();
    //    }
    //}

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

inline void sqlite3_sequence::set_create_table_cb(
    const create_table_cb_t& _cb)
{
    std::lock_guard<std::mutex> locker(mtx_);
    create_table_cb_ = std::make_shared<create_table_cb_t>(_cb);
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
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    mgpp::err err;
    do {
        auto result = get_rw_hdl(_index, _node, _create_if_not_exist);
        if (result) {
            return result;
        }
        
        std::this_thread::yield();
        err = result.error();
    } while (std::chrono::system_clock::now() - start > _ms);
    
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
    std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
    mgpp::err err;
    do {
        auto result = get_ro_hdl(_index, _node);
        if (result) {
            return result;
        }
        std::this_thread::yield();
        err = result.error();
    } while (std::chrono::system_clock::now() - start > _ms);
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

        if (_create_if_not_exist)
        {
            iit = index_infos_.emplace(_index, std::make_shared<__index_info>()).first;
        }
        else {
            return outcome::failure(mgpp::err{ MGEC__NOENT, "index not exists" });
        }    
    }
    index_info = iit->second;
    locker.unlock();

    std::unique_lock<std::mutex> index_locker(index_info->mtx_);
    __node_info_sptr node_info;
    bool has_create_node = false;
    auto nit = index_info->nodes_.find(_node);
    if (nit == index_info->nodes_.end()) {

        auto node_u8path = filepath(_index, _node);
        if (!ghc::filesystem::exists(mm_to<memepp::native_string>(node_u8path)) && !_create_if_not_exist)
        {
            return outcome::failure(mgpp::err{ MGEC__NOENT, "node not exists" });
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
    if (node_info->db_status_ == db_status_t::wait_for_delete) 
    {
        if (node_info->is_hdl_referenced())
        {
            return outcome::failure(mgpp::err{ MGEC__AGAIN, "db is wait for delete" });
        }

        do {
            sqlite3_hdl_sptr ro_hdl;
            sqlite3_hdl_sptr rw_hdl;
            if (node_info->has_hdl())
            {
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
            }
            node_locker.unlock();
        } while (0);
        
        if (has_create_node) {
            index_locker.lock();
            index_info->nodes_.erase(_node);
            index_locker.unlock();
        }

        std::error_code ec;
        ghc::filesystem::remove(mm_to<memepp::native_string>(filepath(_index, _node)), ec);
        if (ec) {
            // TODO: log
        }
        return outcome::failure(mgpp::err{ MGEC__AGAIN, "db is wait for delete" });
    }

    if (node_info->hdl_status_ == hdl_status_t::unavail) 
    {
        return outcome::failure(mgpp::err{ MGEC__AGAIN, "hdl is unavail" });
    }

    if (node_info->hdl_status_ == hdl_status_t::busy ||
        node_info->hdl_status_ == hdl_status_t::creating) 
    {
        return outcome::failure(mgpp::err{ MGEC__BUSY, "hdl is busy" });
    }

    if (node_info->hdl_status_ == hdl_status_t::wait_for_close)
    {
        if (node_info->is_hdl_referenced())
        {
            return outcome::failure(mgpp::err{ MGEC__AGAIN, "hdl is wait for close" });
        }

        do {
            sqlite3_hdl_sptr ro_hdl;
            sqlite3_hdl_sptr rw_hdl;
            if (node_info->has_hdl())
            {
                ro_hdl.swap(node_info->s_ro_hdl_);
                rw_hdl.swap(node_info->s_rw_hdl_);
            }
            node_locker.unlock();
        } while (0);

        if (has_create_node) {
            index_locker.lock();
            index_info->nodes_.erase(_node);
            index_locker.unlock();
        }
        return outcome::failure(mgpp::err{ MGEC__AGAIN, "hdl is wait for close" });
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
    
    node_info->hdl_status_ = hdl_status_t::creating;
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
        bool has_other_hdl = (node_info->is_hdl_referenced());
        node_info->hdl_status_ = hdl_status_t::ok;
        node_locker.unlock();
        if (has_create_node && !has_other_hdl) 
        {
            index_locker.lock();
            index_info->nodes_.erase(_node);
            index_locker.unlock();
        }
        
        return outcome::failure(hdl_ret.error());
    }

    locker.lock();
    auto create_table_cb = create_table_cb_;
    auto table_name = table_name_;
    locker.unlock();

    try {
        if (!_is_readonly && create_table_cb)
            (*create_table_cb)(hdl_ret.value(), table_name_, _index, _node);
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
    node_info->set_hdl(_is_readonly, hdl_ret.value());

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

    auto dir_path = mm_to<memepp::native_string>(dir_u8path);
    auto dir_iter = ghc::filesystem::directory_iterator(dir_path);
    auto dir_end  = ghc::filesystem::directory_iterator();

    for (; dir_iter != dir_end; ++dir_iter) 
    {
        if (!ghc::filesystem::is_directory(dir_iter->status())) 
            continue;

        auto index_name = dir_iter->path().filename().string();
        auto index_iter = ghc::filesystem::directory_iterator(dir_iter->path());
        
        auto index_id = atoll(index_name.data());
        for (; index_iter != dir_end; ++index_iter) 
        {
            if (!ghc::filesystem::is_regular_file(index_iter->status())) 
                continue;

            auto node_name = index_iter->path().filename().string();
            auto node_path = index_iter->path();

            std::vector<memepp::string_view> node_name_parts;
            memepp::split(memepp::view(node_name), ".", 
                memepp::split_behavior_t::skip_empty_parts, std::back_inserter(node_name_parts));

            if (node_name_parts.size() != 3)
                continue;
            
            auto node_id = atoll(node_name_parts[1].to_string().data());

            all_nodes[node_id].emplace(index_id);
            ++total_count;
            total_kb += (ghc::filesystem::file_size(node_path) / 1024);
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
    for (auto& iit : dels) 
    {
        auto index_id = iit.first;
        auto& nodes = iit.second;

        std::unique_lock<std::mutex> locker(mtx_);
        auto iit = index_infos_.find(index_id);
        if (iit == index_infos_.end()) {
            locker.unlock();
            for (auto node_id : nodes) {
                std::error_code ec;
                ghc::filesystem::remove(mm_to<memepp::native_string>(filepath(index_id, node_id)), ec);
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
                std::error_code ec;
                ghc::filesystem::remove(mm_to<memepp::native_string>(filepath(index_id, node_id)), ec);
                ++count;
                continue;
            }

            auto node_info = nit->second;
            index_locker.unlock();

            std::unique_lock<std::mutex> node_locker(node_info->mtx_);
            if (node_info->is_hdl_referenced())
            {
                node_info->db_status_  = db_status_t::wait_for_delete;
                node_info->hdl_status_ = hdl_status_t::unavail;
                node_locker.unlock();
                continue;
            }

            do {
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                if (node_info->has_hdl())
                {
                    ro_hdl.swap(node_info->s_ro_hdl_);
                    rw_hdl.swap(node_info->s_rw_hdl_);
                }
                node_locker.unlock();
            } while (0);
            
            std::error_code ec;
            ghc::filesystem::remove(mm_to<memepp::native_string>(filepath(index_id, node_id)), ec);
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

    auto dir_path = mm_to<memepp::native_string>(dir_u8path);
    auto dir_iter = ghc::filesystem::directory_iterator(dir_path);
    auto dir_end  = ghc::filesystem::directory_iterator();

    count_t count = 0;
    for (; dir_iter != dir_end; ++dir_iter) 
    {
        if (!ghc::filesystem::is_directory(dir_iter->status())) 
            continue;

        auto index_name = dir_iter->path().filename().string();
        auto index_iter = ghc::filesystem::directory_iterator(dir_iter->path());
        
        std::map<node_id_t, std::string> node_paths;
        for (; index_iter != dir_end; ++index_iter) 
        {
            if (!ghc::filesystem::is_regular_file(index_iter->status())) 
                continue;

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
                std::error_code ec;
                ghc::filesystem::remove(path.second, ec);
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
                std::error_code ec;
                ghc::filesystem::remove(path.second, ec);
                ++count;
                continue;
            }

            auto node_info = nit->second;
            index_locker.unlock();

            std::unique_lock<std::mutex> node_locker(node_info->mtx_);
            if (node_info->is_hdl_referenced())
            {
                node_info->db_status_  = db_status_t::wait_for_delete;
                node_info->hdl_status_ = hdl_status_t::unavail;
                node_locker.unlock();
                continue;
            }

            do {
                sqlite3_hdl_sptr ro_hdl;
                sqlite3_hdl_sptr rw_hdl;
                if (node_info->has_hdl()) {
                    ro_hdl.swap(node_info->s_ro_hdl_);
                    rw_hdl.swap(node_info->s_rw_hdl_);
                }
                node_locker.unlock();
            } while (0);
            
            if (ghc::filesystem::exists(path.second))
            {
                std::error_code ec;
                ghc::filesystem::remove(path.second, ec);
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

    for (bool is_index_loop = true; is_index_loop;) 
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
            is_index_loop = false;
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

        for (bool is_node_loop = true; is_node_loop;) 
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
                is_node_loop = false;
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
            
            if (mgu_timestamp_get() - node_info->last_access_ts_ < max_idle_second_ * 1000) 
            {
                continue;
            }

            node_info->s_ro_hdl_.reset();
            node_info->s_rw_hdl_.reset();
            //node_info->hdl_status_ = hdl_status_t::unavail;
            node_locker.unlock();

            //index_locker.lock();
            //index_info->nodes_.erase(curr_node_id);
            //index_locker.unlock();
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

    if (!ghc::filesystem::exists(__temp_directory))
    {
        std::error_code ec;
        ghc::filesystem::create_directories(__temp_directory, ec);
        if (ec) {
            return mgpp::err{ MGEC__ERR, "failed to create temp directory" };
        }
    }

    //sqlite3_temp_directory = __temp_directory;
#endif
    return mgpp::err::make_ok();
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
    if (data->is_readonly_) {
        node_info->s_ro_hdl_.reset();
    }
    else {
        node_info->s_rw_hdl_.reset();
    }
    bool has_hdl = node_info->is_hdl_referenced() && node_info->has_hdl();
    bool db_delete = (node_info->db_status_ == db_status_t::wait_for_delete);
    node_locker.unlock();

    if (!has_hdl && db_delete)
    {
        std::error_code ec;
        ghc::filesystem::remove(mm_to<memepp::native_string>(seq->filepath(data->index_id_, data->node_id_)), ec);
        if (ec) {
            // TODO: log
        }
    }

    if (has_hdl) {
        return;
    }

    index_locker.lock();
    index_info->nodes_.erase(data->node_id_);
    index_locker.unlock();
}

}
} // namespace mmbkpp

#endif // !MMBKPP_STRG_SQLITE3_SEQUENCE_HPP_INCLUDED
