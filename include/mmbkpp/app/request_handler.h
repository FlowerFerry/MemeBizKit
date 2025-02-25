
#ifndef MMBKPP_APP_REQUEST_HANDLER_H_INCLUDED
#define MMBKPP_APP_REQUEST_HANDLER_H_INCLUDED

#include <map>
#include <tuple>
#include <deque>
#include <functional>
#include <type_traits>

#include <mego/util/std/time.h>
#include <stdint.h>

#include <megopp/err/err.h>
#include <megopp/help/null_mutex.h>
#include <megopp/util/scope_locker.h>
#include <mmbkpp/chrono/passive_timer.hpp>
#include <mmbkpp/log/level.h>

namespace mmbkpp {
namespace app {

enum class reqhdlr_retry_state_t : uint8_t
{
    first = 0,
    retry = 1,
};

using reqhdlr_request_id_t = mmint_t;

template<typename _RequestObject, typename _ResponseObject, 
    typename _Mutex = mgpp::help::null_mutex>
struct request_handler 
{
    using retry_state_t = reqhdlr_retry_state_t;
    using request_object_t  = _RequestObject;
    using response_object_t = _ResponseObject;
    using request_id_t  = reqhdlr_request_id_t;
    using request_cb_t  = 
        std::function<mgpp::err(request_id_t, retry_state_t, const request_object_t&)>;
    using response_cb_t =
        std::function<mgpp::err(request_id_t, const request_object_t&, const response_object_t*, const mgpp::err&)>;


    inline void set_serialize_flag(bool _flag)
    {
        std::unique_lock locker{ mutex_ };
        serialize_ = _flag;
    }

    inline void set_request_cb(const request_cb_t& _cb)
    {
        std::unique_lock locker{ mutex_ };
        request_cb_ = _cb;
    }

    inline void set_response_cb(const response_cb_t& _cb)
    {
        std::unique_lock locker{ mutex_ };
        response_cb_ = _cb;
    }

    inline void set_timeout(mmint_t _timeout)
    {
        std::unique_lock locker{ mutex_ };
        timeout_ms_ = _timeout;
    }

    inline void set_max_queue(mmint_t _max_queue)
    {
        std::unique_lock locker{ mutex_ };
        max_queue_ = _max_queue;
    }

    inline void set_max_retry(size_t  _max_retry)
    {
        std::unique_lock locker{ mutex_ };
        max_retry_ = _max_retry;
    }

    inline std::tuple<request_id_t, mgpp::err> enqueue(request_object_t _req)
    {
        std::unique_lock locker{ mutex_ };
        return enqueue(std::move(_req), -1, locker);
    }
    
    inline std::tuple<request_id_t, mgpp::err> enqueue(request_object_t _req, mmint_t _timeout_ms)
    {
        std::unique_lock locker{ mutex_ };
        return enqueue(std::move(_req), _timeout_ms, locker);
    }

    inline std::tuple<request_id_t, mgpp::err> enqueue(
        request_object_t _req, mmint_t _timeout_ms, std::unique_lock<_Mutex>& _lock)
    {
        mgpp::util::scope_unique_locker<_Mutex> locker{ _lock };
        if (req_queue_.size() >= __max_queue__sync()) 
        {
            locker.unlock();
            return std::make_tuple(0, mgpp::err{ MGEC__ERR });
        }

        auto reqid  = __get_next_id__sync();
        auto ms     = __timeout__sync();
        auto ticker = ticker_;
        locker.unlock();
        auto req = std::make_shared<request_t>();
        req->id  = reqid;
        req->obj = std::move(_req);
        req->timer.set_ticker(ticker);
        if (_timeout_ms > 0)
            ms = _timeout_ms;
        req->timer.set_interval(ms);
        req->timeout_cb = [this, reqid]() 
        { 
            response_failure(reqid, mgpp::err{ MGEC__TIMEDOUT }); 
            return true;
        };
        locker.lock();
        req_queue_.push_back(req);
        return std::make_tuple(reqid, mgpp::err{});
    }

    inline mgpp::err response_success(request_id_t _id, const response_object_t& _resp)
    {
        std::unique_lock locker{ mutex_ };
        return response_success(_id, _resp, locker);
    }

    inline mgpp::err response_success(request_id_t _id, const response_object_t& _resp, std::unique_lock<_Mutex>& _lock)
    {
        mgpp::util::scope_unique_locker<_Mutex> locker{ _lock };
        auto it = req_wait_map_.find(_id);
        if (it == req_wait_map_.end())
            return mgpp::err{ MGEC__ERR };

        auto req = it->second;
        auto cb  = response_cb_;
        req->timer.cancel();
        req_wait_map_.erase(it);
        locker.unlock();

        if (cb)
            return cb(_id, req->obj, &_resp, {});

        return {};
    }

    inline mgpp::err response_failure(request_id_t _id, const mgpp::err& _err)
    {
        std::unique_lock locker{ mutex_ };
        return response_failure(_id, _err, locker);
    }

    inline mgpp::err response_failure(request_id_t _id, const mgpp::err& _err, std::unique_lock<_Mutex>& _lock)
    {
        mgpp::util::scope_unique_locker<_Mutex> locker{ _lock };
        auto it = req_wait_map_.find(_id);
        if (it == req_wait_map_.end())
            return mgpp::err{ MGEC__ERR };
        
        auto req = it->second;
        req_wait_map_.erase(it);
        locker.unlock();

        req->timer.cancel();

        if (req->retry++ < __max_retry__sync())
        {
            req_queue_.push_back(req);
            return {};
        }

        locker.lock();
        auto cb = response_cb_;
        locker.unlock();

        if (cb)
            return cb(_id, req->obj, nullptr, _err);
        
        return {};
    }

    inline mmint_t poll_check(mgu_timestamp_t _now) const
    {
        std::unique_lock locker{ mutex_ };
        return poll_check(_now, locker);
    }

    inline mmint_t poll_check(mgu_timestamp_t _now, std::unique_lock<_Mutex>& _lock) const
    {
        mgpp::util::scope_unique_locker<_Mutex> locker{ _lock };
        if (req_queue_.size() && req_wait_map_.empty())
            return 0;
        locker.unlock();

        // auto iv = ticker_->due_in(_now);
        // if (iv > 0)
        //     return mmint_t(iv);
        return 0;
    }

    inline mgpp::err poll(mgu_timestamp_t _now)
    {
        std::unique_lock locker{ mutex_ };
        return poll(_now, locker);
    }

    mgpp::err poll(mgu_timestamp_t _now, std::unique_lock<_Mutex>& _lock)
    {

        mgpp::err err;
        mgpp::util::scope_unique_locker<_Mutex> locker{ _lock, std::defer_lock_t{} };
        // err = __handle_response_queue__sync(locker);
        // if (err)
        //     return err;

        // locker.unlock();

        ticker_->wheel_timing(_now);

        err = __handle_request_queue__sync(locker);
        if (err)
            return err;

        return {};
    }

private:

    inline constexpr mmint_t __timeout__sync() const noexcept
    {
        return timeout_ms_;
    }

    inline constexpr mmint_t __max_queue__sync() const noexcept
    {
        return max_queue_;
    }

    inline constexpr size_t __max_retry__sync() const noexcept
    {
        return max_retry_;
    }

    inline bool __is_serialize__sync() const noexcept
    {
        return serialize_;
    }

    inline constexpr size_t __get_next_id__sync() noexcept
    {
        if (++curr_id_idx_ == 0)
            curr_id_idx_ = 1;
        return curr_id_idx_;
    }

    inline void __log(log::level, const std::string&) const noexcept
    {
        // TODO
    }

    inline mgpp::err __handle_request_queue__sync(mgpp::util::scope_unique_locker<_Mutex>& _locker)
    {
        _locker.lock();
        if (req_queue_.empty())
            return {};
        if (__is_serialize__sync() && !req_wait_map_.empty())
            return {};

        auto req = req_queue_.front();
        req_queue_.pop_front();

        auto req_cb = request_cb_;
        _locker.unlock();

        if (!req_cb) {
            _locker.lock();
            auto resp_cb = response_cb_;
            _locker.unlock();
            if (resp_cb)
                return resp_cb(req->id, req->obj, nullptr, mgpp::err{ MGEC__INVAL });
            return { MGEC__INVAL };
        }

        auto ts = mgu_timestamp_get();
        req->timer.start_once(ts);

        auto err = req_cb(req->id, req->retry ? retry_state_t::retry : retry_state_t::first, req->obj);
        if (err) {
            req->timer.cancel();
            _locker.lock();
            auto resp_cb = response_cb_;
            _locker.unlock();
            if (resp_cb)
                return resp_cb(req->id, req->obj, nullptr, err);

            return err;
        }
        
        _locker.lock();
        req_wait_map_[req->id] = req;
        return {};
    }

    struct request_t
    {
        request_t()
        {
            timer.on(__on_timeout, this);
        }
        
        static bool __on_timeout(mmbkpp::chrono::passive_timer*, void* _u)
        {
            auto self = reinterpret_cast<request_t*>(_u);
            if (self->timeout_cb)
                return self->timeout_cb();
            return true;
        }

        request_id_t id = 0;
        size_t retry = 0;
        mmint_t timeout_ms_ = -1;
        mmbkpp::chrono::passive_timer timer;
        std::function<bool()> timeout_cb;
        request_object_t obj;
    };
    using request_ptr_t = std::shared_ptr<request_t>;

    struct response_t
    {
        request_id_t id = 0;
        std::unique_ptr<response_object_t> resp;
        mgpp::err err;
    };
    using response_ptr_t = std::shared_ptr<response_t>;

    mutable _Mutex mutex_;
    request_cb_t  request_cb_;
    response_cb_t response_cb_;
    mmint_t       timeout_ms_  = 3000;
    mmint_t       max_queue_   = 1000;
    size_t        max_retry_   = 3;
    size_t        curr_id_idx_ = 0;
    bool serialize_ = false;
    mmbkpp::chrono::ticker_ptr ticker_ = std::make_shared<mmbkpp::chrono::ticker>();
    std::deque<request_ptr_t>  req_queue_;
    std::deque<response_ptr_t> resp_queue_;
    std::map<request_id_t, request_ptr_t> req_wait_map_;
};

}
}

#endif // !MMBKPP_APP_REQUEST_HANDLER_H_INCLUDED
