
#ifndef MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED
#define MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED

#include "option.hpp"

#include <uv.h>

#include <megopp/err/err.h>
#include <megopp/util/scope_cleanup.h>
#include <megopp/util/simple_counter.h>
#include <memepp/convert/std/string.hpp>
#include <memepp/convert/fmt.hpp>
#include <memepp/convert/common.hpp>

#include <mutex>
#include <atomic>
#include <variant>
#include <functional>
#include <chrono>

#include <fmt/format.h>
#include <outcome/result.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;

#undef __on_failure

namespace mmbkpp {
namespace paho_mqtt {
namespace async {

class uvbasic_client : public std::enable_shared_from_this<uvbasic_client>
{
    uvbasic_client(const create_native_options& _opts);

    mgpp::err init(uv_loop_t* _loop);

public:
    struct connect_status
    {
        enum {
            disconnected,
            connecting,
            connected,
            disconnecting
        };
    
        std::atomic_int value = disconnected;
    };
    
    typedef int  message_arrived_cb_t(const char*, int, MQTTAsync_message*);
    typedef void delivery_complete_cb_t(MQTTAsync_token);
    typedef void connect_lost_cb_t(char*);
    typedef void connected_cb_t(char*);
    typedef void disconnected_cb_t(MQTTProperties*, enum MQTTReasonCodes);
    typedef void update_connect_options_cb_t(MQTTAsync_connectData*);
    typedef void success_cb_t (MQTTAsync_successData* );
    typedef void failure_cb_t (MQTTAsync_failureData* );
    typedef void success5_cb_t(MQTTAsync_successData5*);
    typedef void failure5_cb_t(MQTTAsync_failureData5*);

    typedef int ssl_error_cb_t(const char*, size_t);
    typedef unsigned int ssl_psk_cb_t(const char*, char*, unsigned int, unsigned char*, unsigned int);

    using success_data_t = std::variant<MQTTAsync_successData*, MQTTAsync_successData5*>;
    using failure_data_t = std::variant<MQTTAsync_failureData*, MQTTAsync_failureData5*>;

    using log_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, log_level, const memepp::string&)>;

    using message_arrived_callback   = std::function<int (const std::weak_ptr<uvbasic_client>&, const memepp::string_view&, MQTTAsync_message*)>;
    using delivery_complete_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, MQTTAsync_token)>;

    using connect_lost_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, char*)>;
    using connected_callback    = std::function<void(const std::weak_ptr<uvbasic_client>&, char*)>;
    using disconnected_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, MQTTProperties*, enum MQTTReasonCodes)>;
    using reconnected_callback  = std::function<void(const std::weak_ptr<uvbasic_client>&)>;

    // Called periodically while Paho is retrying reconnection in the background.
    // elapsed_sec: seconds since the connection was lost / reconnect attempt started.
    using reconnect_stalled_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int64_t elapsed_sec)>;

    using update_connect_options_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, MQTTAsync_connectData*)>;

    using success_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const success_data_t&)>;
    using failure_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const failure_data_t&)>;

    using connect_success_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const success_data_t&)>;
    using connect_failure_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const failure_data_t&)>;

    using disconnect_success_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const success_data_t&)>;
    using disconnect_failure_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const failure_data_t&)>;

    using subscribe_success_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const success_data_t&)>;
    using subscribe_failure_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const failure_data_t&)>;

    using unsubscribe_success_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const success_data_t&)>;
    using unsubscribe_failure_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const failure_data_t&)>;

    using publish_success_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const success_data_t&)>;
    using publish_failure_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const failure_data_t&)>;

    using ssl_error_callback = std::function<int(const std::weak_ptr<uvbasic_client>&, const char*, size_t)>;
    using ssl_psk_callback   = std::function<unsigned int(const std::weak_ptr<uvbasic_client>&, const char*, char*, unsigned int, unsigned char*, unsigned int)>;

    ~uvbasic_client();

    void destroy_request();

    void set_log_level(log_level _level);
    void set_log_callback(const log_callback& _cb);

    void set_message_arrived_callback(const message_arrived_callback& _cb);
    void set_delivery_complete_callback(const delivery_complete_callback& _cb);

    void set_connect_lost_callback(const connect_lost_callback& _cb);
    void set_connected_callback(const connected_callback& _cb);
    void set_disconnected_callback(const disconnected_callback& _cb);
    void set_reconnected_callback(const reconnected_callback& _cb);
    void set_reconnect_stalled_callback(const reconnect_stalled_callback& _cb);
    void set_health_check_interval(int _seconds);

    // void set_update_connect_options_callback(const update_connect_options_callback& _cb);

    void set_success_callback(const success_callback& _cb);
    void set_failure_callback(const failure_callback& _cb);

    void set_connect_success_callback(const connect_success_callback& _cb);
    void set_connect_failure_callback(const connect_failure_callback& _cb);

    void set_disconnect_success_callback(const disconnect_success_callback& _cb);
    void set_disconnect_failure_callback(const disconnect_failure_callback& _cb);

    void set_subscribe_success_callback(const subscribe_success_callback& _cb);
    void set_subscribe_failure_callback(const subscribe_failure_callback& _cb);

    void set_unsubscribe_success_callback(const unsubscribe_success_callback& _cb);
    void set_unsubscribe_failure_callback(const unsubscribe_failure_callback& _cb);

    void set_publish_success_callback(const publish_success_callback& _cb);
    void set_publish_failure_callback(const publish_failure_callback& _cb);

    void set_ssl_error_callback(const ssl_error_callback& _cb);
    void set_ssl_psk_callback(const ssl_psk_callback& _cb);

    mgpp::err set_conn_opts(const connect_options& _opts);
    mgpp::err set_disconn_opts(const disconnect_options& _opts);
    
    mgpp::err connect();
    mgpp::err disconnect();

    mgpp::err send_message(const memepp::string& _destination_name, const MQTTAsync_message& _msg, MQTTAsync_responseOptions& _opts);

    mgpp::err subscribe  (const memepp::string& _topic, int _qos, MQTTAsync_responseOptions& _opts);
    mgpp::err unsubscribe(const memepp::string& _topic, MQTTAsync_responseOptions& _opts);

    inline constexpr const create_native_options& create_opts() const noexcept { return create_opts_; }
    inline constexpr const connect_native_options& connect_opts() const noexcept { return conn_opts_; }
    inline constexpr const disconnect_native_options& disconnect_opts() const noexcept { return disconn_opts_; }

    inline MQTTAsync native_st() const noexcept { return native_cli_; }
    inline MQTTAsync native_mt() const
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return native_cli_;
    }
    
    inline bool is_connected() const noexcept { auto hdl = native_mt(); return hdl && MQTTAsync_isConnected(hdl); }

protected:
    int  on_message_arrived(char* _topic_name, int _topic_len, MQTTAsync_message* _message);
    void on_delivery_complete(MQTTAsync_token _token);

    void on_connect_lost(char* _cause);
    void on_connected(char* _cause);
    void on_disconnected(MQTTProperties* _response, enum MQTTReasonCodes _reason);

    // void on_update_connect_options(MQTTAsync_connectData* _data);

    void on_success (MQTTAsync_successData * _response);
    void on_failure (MQTTAsync_failureData * _response);
    void on_success5(MQTTAsync_successData5* _response);
    void on_failure5(MQTTAsync_failureData5* _response);

    void on_connect_success (MQTTAsync_successData * _response);
    void on_connect_failure (MQTTAsync_failureData * _response);
    void on_connect_success5(MQTTAsync_successData5* _response);
    void on_connect_failure5(MQTTAsync_failureData5* _response);
    
    void on_disconnect_success (MQTTAsync_successData * _response);
    void on_disconnect_failure (MQTTAsync_failureData * _response);
    void on_disconnect_success5(MQTTAsync_successData5* _response);
    void on_disconnect_failure5(MQTTAsync_failureData5* _response);
    
    void on_subscribe_success (MQTTAsync_successData * _response);
    void on_subscribe_failure (MQTTAsync_failureData * _response);
    void on_subscribe_success5(MQTTAsync_successData5* _response);
    void on_subscribe_failure5(MQTTAsync_failureData5* _response);

    void on_unsubscribe_success (MQTTAsync_successData * _response);
    void on_unsubscribe_failure (MQTTAsync_failureData * _response);
    void on_unsubscribe_success5(MQTTAsync_successData5* _response);
    void on_unsubscribe_failure5(MQTTAsync_failureData5* _response);

    void on_publish_success (MQTTAsync_successData * _response);
    void on_publish_failure (MQTTAsync_failureData * _response);
    void on_publish_success5(MQTTAsync_successData5* _response);
    void on_publish_failure5(MQTTAsync_failureData5* _response);

    int on_ssl_error(const char* _str, size_t _len);
    unsigned int on_ssl_psk(const char* _hint, char* _identity, unsigned int _max_identity_len, unsigned char* _psk, unsigned int _max_psk_len);

    void on_destroy_async_call (uv_async_t * _handle);
    void on_destroy_async_close(uv_handle_t* _handle);

    void on_retry_connect_async_call (uv_async_t * _handle);
    void on_retry_connect_async_close(uv_handle_t* _handle);
    
    void on_retry_connect_cancel_call (uv_async_t * _handle);
    void on_retry_connect_cancel_close(uv_handle_t* _handle);

    void on_retry_connect_timer_call (uv_timer_t * _handle);
    void on_retry_connect_timer_close(uv_handle_t* _handle);

    void on_health_check_timer_call (uv_timer_t * _handle);
    void on_health_check_timer_close(uv_handle_t* _handle);

    void on_destroy();
    
    mgpp::err __connect_mt();
    mgpp::err __disconnect_mt();
    
    //mgpp::err __set_auto_reconnect(bool _b);
    inline constexpr bool __auto_reconnect_enable() const noexcept { return conn_opts_.raw().automaticReconnect != 0; }

    inline bool __auto_reconn_hdl_running_st() const noexcept { return auto_reconn_hdl_running_; }
    inline bool __auto_reconn_hdl_running_mt() const noexcept
    {
        //std::unique_lock<std::mutex> locker(mtx_);
        return auto_reconn_hdl_running_;
    }

    inline void __set_auto_reconn_hdl_running_st(bool _b) noexcept { auto_reconn_hdl_running_ = _b; }
    inline void __set_auto_reconn_hdl_running_mt(bool _b)
    {
        //std::unique_lock<std::mutex> locker(mtx_);
        auto_reconn_hdl_running_ = _b;
    }
public:

    static outcome::checked<std::shared_ptr<uvbasic_client>, mgpp::err> 
        create(
            const create_options& _opts, 
            const connect_options& _conn_opts,
            const disconnect_options& _disconn_opts,
            uv_loop_t* _loop);

    static int __on_message_arrived(void* _context, char* _topic_name, int _topic_len, MQTTAsync_message* _message);

    static void __on_delivery_complete(void* _context, MQTTAsync_token _token);

	static void __on_connect_lost(void *_context, char *_cause);
    static void __on_connected   (void* _context, char* _cause);
    static void __on_disconnected(void* _context, MQTTProperties* _response, enum MQTTReasonCodes _reason);

    // static void __on_update_connect_options(void* _context, MQTTAsync_connectData* _data);

    static void __on_success (void* _context, MQTTAsync_successData * _response);
    static void __on_failure (void* _context, MQTTAsync_failureData * _response);
    static void __on_success5(void* _context, MQTTAsync_successData5* _response);
    static void __on_failure5(void* _context, MQTTAsync_failureData5* _response);

	static void __on_connect_success (void* _context, MQTTAsync_successData * _response);
	static void __on_connect_failure (void* _context, MQTTAsync_failureData * _response);
	static void __on_connect_success5(void* _context, MQTTAsync_successData5* _response);
	static void __on_connect_failure5(void* _context, MQTTAsync_failureData5* _response);

	static void __on_disconnect_success (void* _context, MQTTAsync_successData * _response);
	static void __on_disconnect_failure (void* _context, MQTTAsync_failureData * _response);
	static void __on_disconnect_success5(void* _context, MQTTAsync_successData5* _response);
	static void __on_disconnect_failure5(void* _context, MQTTAsync_failureData5* _response);

    static void __on_subscribe_success (void* _context, MQTTAsync_successData * _response);
    static void __on_subscribe_failure (void* _context, MQTTAsync_failureData * _response);
    static void __on_subscribe_success5(void* _context, MQTTAsync_successData5* _response);
    static void __on_subscribe_failure5(void* _context, MQTTAsync_failureData5* _response);

    static void __on_unsubscribe_success (void* _context, MQTTAsync_successData * _response);
    static void __on_unsubscribe_failure (void* _context, MQTTAsync_failureData * _response);
    static void __on_unsubscribe_success5(void* _context, MQTTAsync_successData5* _response);
    static void __on_unsubscribe_failure5(void* _context, MQTTAsync_failureData5* _response);

    static void __on_publish_success (void* _context, MQTTAsync_successData * _response);
    static void __on_publish_failure (void* _context, MQTTAsync_failureData * _response);
    static void __on_publish_success5(void* _context, MQTTAsync_successData5* _response);
    static void __on_publish_failure5(void* _context, MQTTAsync_failureData5* _response);

    static int __on_ssl_error(const char* _str, size_t _len, void* _context);
    static unsigned int __on_ssl_psk(const char* _hint, char* _identity, unsigned int _max_identity_len, unsigned char* _psk, unsigned int _max_psk_len, void* _context);

    static void __on_destroy_async_call (uv_async_t * _handle);
    static void __on_destroy_async_close(uv_handle_t* _handle);

    static void __on_retry_connect_async_call (uv_async_t * _handle);
    static void __on_retry_connect_async_close(uv_handle_t* _handle);
    
    static void __on_retry_connect_cancel_call (uv_async_t * _handle);
    static void __on_retry_connect_cancel_close(uv_handle_t* _handle);

    static void __on_retry_connect_timer_call (uv_timer_t * _handle);
    static void __on_retry_connect_timer_close(uv_handle_t* _handle);

    static void __on_health_check_timer_call (uv_timer_t * _handle);
    static void __on_health_check_timer_close(uv_handle_t* _handle);

protected:
    
    template<typename... Args>
    inline void _log(log_level _lvl, const char* _fmt, Args&&... _args)
    {
        if (log_cb_) {
            log_cb_(weak_from_this(), _lvl, mm_from(fmt::format(_fmt, std::forward<Args>(_args)...)));
        }
    }
    
    inline void _log(log_level _lvl, const char* _fmt)
    {
        if (log_cb_) {
            log_cb_(weak_from_this(), _lvl, (_fmt));
        }
    }

    mutable std::mutex mtx_;

    std::shared_ptr<void> self_;
    MQTTAsync native_cli_;
    create_native_options create_opts_;
    connect_native_options conn_opts_;
    disconnect_native_options disconn_opts_;

    log_level log_lvl_ = log_level::warn;
    log_callback log_cb_;
    
    std::atomic_bool auto_reconn_hdl_running_ = false;
    std::atomic_bool wait_conn_restored_ = false;
    std::atomic_bool disconnect_requested_ = false;
    connect_status   connect_status_;

    message_arrived_callback message_arrived_cb_;
    delivery_complete_callback delivery_complete_cb_;

    connect_lost_callback connect_lost_cb_;
    connected_callback connected_cb_;
    disconnected_callback disconnected_cb_;
    reconnected_callback reconnected_cb_;
    reconnect_stalled_callback reconnect_stalled_cb_;

    std::shared_ptr<update_connect_options_callback> update_connect_options_cb_;
    
    std::shared_ptr<success_callback> success_cb_;
    std::shared_ptr<failure_callback> failure_cb_;
    
    connect_success_callback connect_success_cb_;
    connect_failure_callback connect_failure_cb_;
    
    disconnect_success_callback disconnect_success_cb_;
    disconnect_failure_callback disconnect_failure_cb_;

    subscribe_success_callback subscribe_success_cb_;
    subscribe_failure_callback subscribe_failure_cb_;

    unsubscribe_success_callback unsubscribe_success_cb_;
    unsubscribe_failure_callback unsubscribe_failure_cb_;

    publish_success_callback publish_success_cb_;
    publish_failure_callback publish_failure_cb_;

    ssl_error_callback ssl_error_cb_;
    ssl_psk_callback ssl_psk_cb_;

    megopp::help::null_mutex nullmtx_;
    mgpp::util::ref_counter<> handle_counter_;
    std::unique_ptr<uv_async_t> destroy_async_req_;

    std::unique_ptr<uv_async_t> retry_connect_async_req_;
    std::unique_ptr<uv_async_t> retry_connect_async_cancel_;
    std::unique_ptr<uv_timer_t> retry_connect_timer_;

    std::unique_ptr<uv_timer_t> health_check_timer_;
    std::chrono::steady_clock::time_point reconnect_start_time_;
    std::atomic_int health_check_interval_sec_{10};

    std::atomic_int retry_connect_backoff_ms_{1000};
    
};

uvbasic_client::uvbasic_client(const create_native_options& _opts)
    : native_cli_(nullptr)
    , create_opts_(_opts)
    , conn_opts_(_opts.raw().MQTTVersion)
    , disconn_opts_(_opts.raw().MQTTVersion)
{
    sizeof(*this);
    connect_status_.value = connect_status::disconnected;

    handle_counter_.set_callback(nullmtx_, [this](auto&)
    {
        on_destroy();
    });

    conn_opts_.raw().context    = this;
    conn_opts_.raw().onSuccess  = __on_connect_success;
    conn_opts_.raw().onFailure  = __on_connect_failure;
    conn_opts_.set_success5_cb(__on_connect_success5);
    conn_opts_.set_failure5_cb(__on_connect_failure5);

    disconn_opts_.raw().context    = this;
    disconn_opts_.raw().onSuccess  = __on_disconnect_success;
    disconn_opts_.raw().onFailure  = __on_disconnect_failure;
    disconn_opts_.raw().onSuccess5 = __on_disconnect_success5;
    disconn_opts_.raw().onFailure5 = __on_disconnect_failure5;

}

uvbasic_client::~uvbasic_client()
{
}

inline mgpp::err uvbasic_client::set_conn_opts(const connect_options& _opts)
{
    std::unique_lock<std::mutex> locker(mtx_);
    
    if (native_cli_ && MQTTAsync_isConnected(native_cli_))
        return mgpp::err{ MGEC__PERM, "already connected" };
    
    if (__auto_reconn_hdl_running_st())
        return mgpp::err{ MGEC__PERM, "auto reconnect is running" };
    
    conn_opts_.assign(_opts);
    
    //if (conn_opts_.raw().automaticReconnect != 0) 
    //{
    //    __set_auto_reconnect(true);
    //}
    //else {
    //    __set_auto_reconnect(false);
    //}
    
    return {};
}

inline mgpp::err uvbasic_client::set_disconn_opts(const disconnect_options& _opts)
{
    std::unique_lock<std::mutex> locker(mtx_);

    if (native_cli_ && MQTTAsync_isConnected(native_cli_))
        return mgpp::err{ MGEC__PERM, "already connected" };

    if (__auto_reconn_hdl_running_st())
        return mgpp::err{ MGEC__PERM, "auto reconnect is running" };

    disconn_opts_.assign(_opts);
    return {};
}

inline mgpp::err uvbasic_client::connect()
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (!destroy_async_req_)
        return mgpp::err{ MGEC__INVALID_HANDLE, "invalid handle" };
    if (!native_cli_)
        return mgpp::err{ MGEC__INVALID_HANDLE, "invalid handle" };

    if (disconnect_requested_.load(std::memory_order_acquire))
        return mgpp::err{ MGEC__INPROGRESS, "a disconnect is in progress, retry later" };

    if (MQTTAsync_isConnected(native_cli_))
        return {};
    
    if (__auto_reconn_hdl_running_st())
        return mgpp::err{ MGEC__INPROGRESS, "auto reconnect is running" };
    locker.unlock();
    
    // Reset backoff for fresh user-initiated connect
    retry_connect_backoff_ms_.store(1000, std::memory_order_release);
    
    auto e = __connect_mt();
    if ( e ) {
        if (__auto_reconnect_enable()) 
        {
            locker.lock();
            if (retry_connect_async_req_)
            {
                __set_auto_reconn_hdl_running_st(true);
                uv_async_send(retry_connect_async_req_.get());
                locker.unlock();
                if (log_lvl_ <= log_level::trace)
                    _log(log_level::trace, 
                        "uvbasic_client({})::connect failed and run auto reconnect; code= {}; desc= {}", 
                        create_opts_.client_id(), e.usercode(), e.message());
            }
            else {
                locker.unlock();
                if (log_lvl_ <= log_level::trace)
                    _log(log_level::trace, 
                        "uvbasic_client({})::connect failed and auto reconnect is not running; code= {}; desc= {}", 
                        create_opts_.client_id(), e.usercode(), e.message());
            }
        }
        else {
            return e;
        }
    }

    return {};
}

inline mgpp::err uvbasic_client::disconnect()
{
    // Phase 1: Set the flag first — all async callbacks check this to change their behaviour
    disconnect_requested_.store(true, std::memory_order_release);
    wait_conn_restored_.store(false, std::memory_order_release);

    // Phase 2: Set the wrapper-layer flag synchronously (atomic, thread-safe),
    // then signal the event-loop thread to stop timers safely.
    // Per libuv docs (design.rst), the event loop and handles are NOT thread-safe
    // except where stated otherwise; uv_async_send is the only API confirmed safe
    // from any thread.
    if (__auto_reconn_hdl_running_st())
        __set_auto_reconn_hdl_running_st(false);

    if (retry_connect_async_cancel_)
        uv_async_send(retry_connect_async_cancel_.get());

    // Phase 3: Route based on current connection state
    int cur = connect_status_.value.load(std::memory_order_acquire);

    if (cur == connect_status::disconnected)
    {
        // Idempotent — already disconnected
        disconnect_requested_.store(false, std::memory_order_release);
        return {};
    }

    if (cur == connect_status::disconnecting)
    {
        // Already in progress
        return mgpp::err{ MGEC__ALREADY, "disconnect already in progress" };
    }

    if (cur == connect_status::connecting)
    {
        // Paho's MQTTAsync_disconnect returns MQTTASYNC_DISCONNECTED when !connected,
        // but it does NOT cancel the in-flight connect.  We just set the flag;
        // on_connect_success / on_connect_failure will handle the remainder.
        if (log_lvl_ <= log_level::trace)
            _log(log_level::trace,
                "uvbasic_client({})::disconnect while connecting — waiting for connect callback",
                create_opts_.client_id());
        return {};
    }

    // cur == connect_status::connected
    return __disconnect_mt();
}

inline mgpp::err uvbasic_client::send_message(const memepp::string& _destination_name, const MQTTAsync_message& _msg, MQTTAsync_responseOptions& _opts)
{
    _opts.context = this;
    if (create_opts_.raw().MQTTVersion < MQTTVERSION_5)
    {
        _opts.onSuccess = __on_publish_success;
        _opts.onFailure = __on_publish_failure;
    }
    else {
        _opts.onSuccess5 = __on_publish_success5;
        _opts.onFailure5 = __on_publish_failure5;
    }

    std::unique_lock<std::mutex> locker(mtx_);
    auto hdl = native_cli_;
    if (!hdl)
        return mgpp::err{ MGEC__PERM, "client not created" };
    locker.unlock();

    int rc = 0;
    if ((rc = MQTTAsync_sendMessage(hdl, _destination_name.data(), &_msg, &_opts)) != MQTTASYNC_SUCCESS)
    {
        return mgpp::err{ MGEC__ERR, rc, "send_message failed" };
    }

    return {};
}

inline mgpp::err uvbasic_client::subscribe(const memepp::string& _topic, int _qos, MQTTAsync_responseOptions& _opts)
{
    _opts.context = this;
    if (create_opts_.raw().MQTTVersion < MQTTVERSION_5)
    {
        _opts.onSuccess = __on_subscribe_success;
        _opts.onFailure = __on_subscribe_failure;
    }
    else {
        _opts.onSuccess5 = __on_subscribe_success5;
        _opts.onFailure5 = __on_subscribe_failure5;
    }

    std::unique_lock locker(mtx_);
    auto hdl = native_cli_;
    if (!hdl)
        return mgpp::err{ MGEC__PERM, "client not created" };
    locker.unlock();

    int rc = 0;
    if ((rc = MQTTAsync_subscribe(hdl, _topic.data(), _qos, &_opts)) != MQTTASYNC_SUCCESS)
    {
        return mgpp::err{ MGEC__ERR, rc, "subscribe failed" };
    }

    return {};
}

inline mgpp::err uvbasic_client::unsubscribe(const memepp::string& _topic, MQTTAsync_responseOptions& _opts)
{
    _opts.context = this;
    if (create_opts_.raw().MQTTVersion < MQTTVERSION_5)
    {
        _opts.onSuccess = __on_unsubscribe_success;
        _opts.onFailure = __on_unsubscribe_failure;
    }
    else {
        _opts.onSuccess5 = __on_unsubscribe_success5;
        _opts.onFailure5 = __on_unsubscribe_failure5;
    }
        
    std::unique_lock locker(mtx_);
    auto hdl = native_cli_;
    if (!hdl)
        return mgpp::err{ MGEC__PERM, "client not created" };
    locker.unlock();

    int rc = 0;
    if ((rc = MQTTAsync_unsubscribe(hdl, _topic.data(), &_opts)) != MQTTASYNC_SUCCESS)
    {
        return mgpp::err{ MGEC__ERR, rc, "unsubscribe failed" };
    }

    return {};
}

inline mgpp::err uvbasic_client::init(uv_loop_t* _loop)
{
    if (!_loop)
        return mgpp::err{ MGEC__INVAL, "invalid loop" };

    std::unique_lock<std::mutex> locker(mtx_);
    if (destroy_async_req_)
        return mgpp::err{ MGEC__ALREADY, "already initialized" };
    locker.unlock();

    MQTTAsync handle;
    auto rc = MQTTAsync_createWithOptions(
        &handle,
        conn_opts_.server_url().data(),
        create_opts_.client_id().data(),
        create_opts_.persistence_type(),
        NULL,
        &create_opts_.raw()
    );
    if (rc != MQTTASYNC_SUCCESS) {
        return mgpp::err{ MGEC__ERR, rc, "MQTTAsync_createWithOptions failed" };
    }
    auto handle_cleanup = megopp::util::scope_cleanup__create(
        [&handle]() { MQTTAsync_destroy(&handle); }
    );

    rc = MQTTAsync_setMessageArrivedCallback(handle, this, __on_message_arrived);
    if (rc != MQTTASYNC_SUCCESS) {
        return mgpp::err{ MGEC__ERR, rc, "MQTTAsync_setMessageArrivedCallback failed" };
    }
    
    rc = MQTTAsync_setConnected(handle, this, __on_connected);
    if (rc != MQTTASYNC_SUCCESS) {
        return mgpp::err{ MGEC__ERR, rc, "MQTTAsync_setConnected failed" };
    }

    rc = MQTTAsync_setDisconnected(handle, this, __on_disconnected);
    if (rc != MQTTASYNC_SUCCESS) {
        return mgpp::err{ MGEC__ERR, rc, "MQTTAsync_setDisconnected failed" };
    }

    rc = MQTTAsync_setConnectionLostCallback(handle, this, __on_connect_lost);
    if (rc != MQTTASYNC_SUCCESS) {
        return mgpp::err{ MGEC__ERR, rc, "MQTTAsync_setConnectionLostCallback failed" };
    }
    
    handle_counter_.set_count(nullmtx_, 5);

    auto destroy_async_hdl = std::make_unique<uv_async_t>();
    uv_async_init(_loop, destroy_async_hdl.get(), __on_destroy_async_call);
    uv_handle_set_data(reinterpret_cast<uv_handle_t*>(destroy_async_hdl.get()), this);

    auto retry_connect_hdl = std::make_unique<uv_async_t>();
    uv_async_init(_loop, retry_connect_hdl.get(), __on_retry_connect_async_call);
    uv_handle_set_data(reinterpret_cast<uv_handle_t*>(retry_connect_hdl.get()), this);

    auto retry_connect_cancel_hdl = std::make_unique<uv_async_t>();
    uv_async_init(_loop, retry_connect_cancel_hdl.get(), __on_retry_connect_cancel_call);
    uv_handle_set_data(reinterpret_cast<uv_handle_t*>(retry_connect_cancel_hdl.get()), this);

    auto retry_connect_timer = std::make_unique<uv_timer_t>();
    uv_timer_init(_loop, retry_connect_timer.get());
    uv_handle_set_data(reinterpret_cast<uv_handle_t*>(retry_connect_timer.get()), this);

    auto health_check_timer = std::make_unique<uv_timer_t>();
    uv_timer_init(_loop, health_check_timer.get());
    uv_handle_set_data(reinterpret_cast<uv_handle_t*>(health_check_timer.get()), this);

    locker.lock();
    handle_cleanup.cancel();
    native_cli_ = handle;
    destroy_async_req_ = std::move(destroy_async_hdl);
    retry_connect_async_req_ = std::move(retry_connect_hdl);
    retry_connect_async_cancel_ = std::move(retry_connect_cancel_hdl);
    retry_connect_timer_ = std::move(retry_connect_timer);
    health_check_timer_ = std::move(health_check_timer);
    self_ = shared_from_this();
    return {};
}

inline void uvbasic_client::destroy_request()
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (!destroy_async_req_)
        return;
    uv_async_send(destroy_async_req_.get());
}

inline void uvbasic_client::set_log_level(log_level _level)
{
    log_lvl_ = _level;
}

inline void uvbasic_client::set_log_callback(const log_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }
    locker.unlock();
    log_cb_ = _cb;
}

inline void uvbasic_client::set_message_arrived_callback(const message_arrived_callback& _cb) 
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }
    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    message_arrived_cb_ = _cb;
}

inline void uvbasic_client::set_delivery_complete_callback(const delivery_complete_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }
    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    delivery_complete_cb_ = _cb;
}

inline void uvbasic_client::set_connect_lost_callback(const connect_lost_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    connect_lost_cb_ = _cb;
}

inline void uvbasic_client::set_connected_callback(const connected_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    connected_cb_ = _cb;
}

inline void uvbasic_client::set_disconnected_callback(const disconnected_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    disconnected_cb_ = _cb;
}

inline void uvbasic_client::set_reconnected_callback(const reconnected_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    reconnected_cb_ = _cb;
}

inline void uvbasic_client::set_reconnect_stalled_callback(const reconnect_stalled_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }
    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    reconnect_stalled_cb_ = _cb;
}

inline void uvbasic_client::set_health_check_interval(int _seconds)
{
    if (_seconds < 1)
        _seconds = 1;
    if (_seconds > 3600)
        _seconds = 3600;
    health_check_interval_sec_.store(_seconds, std::memory_order_release);
}

inline void uvbasic_client::set_success_callback(const success_callback& _cb)
{
    std::shared_ptr<success_callback> cb;
    if (_cb) {
        cb = std::make_shared<success_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    success_cb_ = cb;
}

inline void uvbasic_client::set_failure_callback(const failure_callback& _cb)
{
    std::shared_ptr<failure_callback> cb;
    if (_cb) {
        cb = std::make_shared<failure_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    failure_cb_ = cb;
}

inline void uvbasic_client::set_connect_success_callback(const connect_success_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    connect_success_cb_ = _cb;
}

inline void uvbasic_client::set_connect_failure_callback(const connect_failure_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    connect_failure_cb_ = _cb;
}

inline void uvbasic_client::set_disconnect_success_callback(const disconnect_success_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    disconnect_success_cb_ = _cb;
}

inline void uvbasic_client::set_disconnect_failure_callback(const disconnect_failure_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    disconnect_failure_cb_ = _cb;
}

inline void uvbasic_client::set_subscribe_success_callback(const subscribe_success_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    subscribe_success_cb_ = _cb;
}

inline void uvbasic_client::set_subscribe_failure_callback(const subscribe_failure_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    subscribe_failure_cb_ = _cb;
}

inline void uvbasic_client::set_unsubscribe_success_callback(const unsubscribe_success_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    unsubscribe_success_cb_ = _cb;
}

inline void uvbasic_client::set_unsubscribe_failure_callback(const unsubscribe_failure_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_))
            return;
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    unsubscribe_failure_cb_ = _cb;
}

inline void uvbasic_client::set_publish_success_callback(const publish_success_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_)) {
            return;
        }
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    publish_success_cb_ = _cb;
}

inline void uvbasic_client::set_publish_failure_callback(const publish_failure_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_)) {
            return;
        }
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    publish_failure_cb_ = _cb;
}

inline void uvbasic_client::set_ssl_error_callback(const ssl_error_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_)) {
            return;
        }
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    ssl_error_cb_ = _cb;
}

inline void uvbasic_client::set_ssl_psk_callback(const ssl_psk_callback& _cb)
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        if (MQTTAsync_isConnected(native_cli_)) {
            return;
        }
    }

    if (__auto_reconn_hdl_running_st())
        return;
    locker.unlock();
    ssl_psk_cb_ = _cb;
}

inline int uvbasic_client::on_message_arrived(char* _topic_name, int _topic_len, MQTTAsync_message* _message)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_message_arrived: topic={}", 
            create_opts_.client_id(), mm_view(_topic_name, _topic_len));
    
    auto cleanup = megopp::util::scope_cleanup__create([&] {
        MQTTAsync_freeMessage(&_message);
        MQTTAsync_free(_topic_name);
    });

    if (!message_arrived_cb_)
        return 1;

    auto result = message_arrived_cb_(weak_from_this(), mm_view(_topic_name, _topic_len), _message);
    if (result == 0) {
        cleanup.cancel();
        return 0;
    }
    return 1;
}

inline void uvbasic_client::on_delivery_complete(MQTTAsync_token _token)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_delivery_complete: token=%d", 
            create_opts_.client_id(), static_cast<int>(_token));
    
    if (delivery_complete_cb_)
        delivery_complete_cb_(weak_from_this(), _token);
}

inline void uvbasic_client::on_connect_lost(char* _cause)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_lost",
            create_opts_.client_id());

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        // User requested disconnect; don't start reconnect, just notify
        if (connect_lost_cb_)
            connect_lost_cb_(weak_from_this(), _cause);
        return;
    }

    if (conn_opts_.raw().automaticReconnect != 0)
    {
        wait_conn_restored_.store(true, std::memory_order_release);
        // Paho's startConnectRetry handles the actual reconnection;
        // shouldBeConnected is still 1 since user didn't call MQTTAsync_disconnect.
        // Update status to reflect that a reconnect attempt is in progress.
        connect_status_.value.store(connect_status::connecting, std::memory_order_release);

        // Start health-check timer for periodic reconnect-stalled notifications
        // and MQTTAsync_isConnected polling (Plans B + D).
        reconnect_start_time_ = std::chrono::steady_clock::now();
        if (health_check_timer_)
            uv_timer_start(health_check_timer_.get(), __on_health_check_timer_call,
                           health_check_interval_sec_.load(std::memory_order_acquire) * 1000, 0);
    }
    else
    {
        // No auto-reconnect — must update the state machine
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    }

    if (connect_lost_cb_)
        connect_lost_cb_(weak_from_this(), _cause);
}

inline void uvbasic_client::on_connected(char* _cause)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connected",
            create_opts_.client_id());

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        // User called disconnect() while auto-reconnect was in progress;
        // on_connect_success has already handled the disconnect path
        // (set connect_status_=disconnecting + called MQTTAsync_disconnect).
        // No-op here to avoid a duplicate MQTTAsync_disconnect call.
        return;
    }

    if (wait_conn_restored_.load(std::memory_order_acquire)) {
        wait_conn_restored_.store(false, std::memory_order_release);

        // Stop the health-check timer — reconnect has succeeded
        if (health_check_timer_)
            uv_timer_stop(health_check_timer_.get());

        if (connect_status_.value.load(std::memory_order_acquire) == connect_status::connecting)
            connect_status_.value.store(connect_status::connected, std::memory_order_release);
        
        if (log_lvl_ <= log_level::trace)
            _log(log_level::trace, "uvbasic_client({})::on_reconnected",
                create_opts_.client_id());
        
        if (reconnected_cb_)
            reconnected_cb_(weak_from_this());
    }

    if (connected_cb_)
        connected_cb_(weak_from_this(), _cause);
}

inline void uvbasic_client::on_disconnected(MQTTProperties* _response, enum MQTTReasonCodes _reason)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_disconnected: reason={}",
            create_opts_.client_id(), static_cast<int>(_reason));

    // Paho calls this ONLY on server-initiated DISCONNECT (MQTT V5).
    // It is NOT called for user-initiated disconnect or TCP connection loss.

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        // User-initiated disconnect is in flight; on_disconnect_success will handle
        // the state transition.  We just notify the user callback here.
        if (disconnected_cb_)
            disconnected_cb_(weak_from_this(), _response, _reason);
        return;
    }

    // Server-initiated disconnect — must update the state machine
    connect_status_.value.store(connect_status::disconnected, std::memory_order_release);

    // Note: Paho will subsequently call on_connect_lost (via nextOrClose),
    // then startConnectRetry if automaticReconnect is enabled and shouldBeConnected is true.
    // Our on_connect_lost will handle wait_conn_restored_ for the reconnect case.

    if (disconnected_cb_)
        disconnected_cb_(weak_from_this(), _response, _reason);
}

inline void uvbasic_client::on_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_success",
            create_opts_.client_id());

    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_failure",
            create_opts_.client_id());

    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_success5",
            create_opts_.client_id());

    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_failure5",
            create_opts_.client_id());
    
    
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_connect_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_success",
            create_opts_.client_id());

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        // User called disconnect() while connecting; we've just connected — disconnect now.
        connect_status_.value.store(connect_status::disconnecting, std::memory_order_release);
        // Lock to safely read native_cli_ — this callback runs on Paho's internal thread,
        // while on_destroy() may concurrently write native_cli_ = nullptr under mtx_.
        MQTTAsync hdl;
        {
            std::unique_lock<std::mutex> locker(mtx_);
            hdl = native_cli_;
        }
        if (hdl)
        {
            int rc = MQTTAsync_disconnect(hdl, &disconn_opts_.raw());
            if (rc != MQTTASYNC_SUCCESS)
            {
                // Paho returned a synchronous error (e.g. MQTTASYNC_DISCONNECTED because
                // m->c->connected==0).  No async callback will fire, so clean up here
                // to prevent disconnect_requested_ from being stuck forever.
                connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
                disconnect_requested_.store(false, std::memory_order_release);
            }
        }
        // Don't call connect_success_cb_ — the user expects a disconnect result
        return;
    }

    connect_status_.value.store(connect_status::connected, std::memory_order_release);

    // Reset backoff on successful connection
    retry_connect_backoff_ms_.store(1000, std::memory_order_release);

    if (connect_success_cb_)
        connect_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_connect_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_failure",
            create_opts_.client_id());

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        // User called disconnect() while connecting; connect failed — end cleanly.
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
        disconnect_requested_.store(false, std::memory_order_release);
        // Don't call connect_failure_cb_ — the user expects a disconnect result
        return;
    }

    if (conn_opts_.raw().automaticReconnect != 0)
    {
        wait_conn_restored_.store(true, std::memory_order_release);

        // Start health-check timer for periodic reconnect-stalled notifications
        // and MQTTAsync_isConnected polling (Plans B + D).
        reconnect_start_time_ = std::chrono::steady_clock::now();
        if (health_check_timer_)
            uv_timer_start(health_check_timer_.get(), __on_health_check_timer_call,
                           health_check_interval_sec_.load(std::memory_order_acquire) * 1000, 0);
    }
    else
    {
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    }

    if (connect_failure_cb_)
        connect_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_connect_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_success5",
            create_opts_.client_id());

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        connect_status_.value.store(connect_status::disconnecting, std::memory_order_release);
        // Lock to safely read native_cli_ — this callback runs on Paho's internal thread,
        // while on_destroy() may concurrently write native_cli_ = nullptr under mtx_.
        MQTTAsync hdl;
        {
            std::unique_lock<std::mutex> locker(mtx_);
            hdl = native_cli_;
        }
        if (hdl)
        {
            int rc = MQTTAsync_disconnect(hdl, &disconn_opts_.raw());
            if (rc != MQTTASYNC_SUCCESS)
            {
                // Paho returned a synchronous error (e.g. MQTTASYNC_DISCONNECTED because
                // m->c->connected==0).  No async callback will fire, so clean up here
                // to prevent disconnect_requested_ from being stuck forever.
                connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
                disconnect_requested_.store(false, std::memory_order_release);
            }
        }
        return;
    }

    connect_status_.value.store(connect_status::connected, std::memory_order_release);

    // Reset backoff on successful connection
    retry_connect_backoff_ms_.store(1000, std::memory_order_release);

    if (connect_success_cb_)
        connect_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_connect_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_failure5",
            create_opts_.client_id());

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
        disconnect_requested_.store(false, std::memory_order_release);
        return;
    }

    if (conn_opts_.raw().automaticReconnect != 0)
    {
        wait_conn_restored_.store(true, std::memory_order_release);

        // Start health-check timer for periodic reconnect-stalled notifications
        // and MQTTAsync_isConnected polling (Plans B + D).
        reconnect_start_time_ = std::chrono::steady_clock::now();
        if (health_check_timer_)
            uv_timer_start(health_check_timer_.get(), __on_health_check_timer_call,
                           health_check_interval_sec_.load(std::memory_order_acquire) * 1000, 0);
    }
    else
    {
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    }

    if (connect_failure_cb_)
        connect_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_disconnect_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_disconnect_success",
            create_opts_.client_id());

    connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    disconnect_requested_.store(false, std::memory_order_release);

    if (disconnect_success_cb_)
        disconnect_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_disconnect_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_disconnect_failure",
            create_opts_.client_id());

    // Paho currently never calls onFailure for disconnect, but if a future version
    // does, treat it as disconnected and clean up — don't leave the state stuck.
    connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    disconnect_requested_.store(false, std::memory_order_release);

    if (disconnect_failure_cb_)
        disconnect_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_disconnect_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_disconnect_success5",
            create_opts_.client_id());

    connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    disconnect_requested_.store(false, std::memory_order_release);

    if (disconnect_success_cb_)
        disconnect_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_disconnect_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_disconnect_failure5",
            create_opts_.client_id());

    // Paho currently never calls onFailure for disconnect, but if a future version
    // does, treat it as disconnected and clean up — don't leave the state stuck.
    connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    disconnect_requested_.store(false, std::memory_order_release);

    if (disconnect_failure_cb_)
        disconnect_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_subscribe_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_subscribe_success",
            create_opts_.client_id());
    
    if (subscribe_success_cb_)
        subscribe_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_subscribe_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_subscribe_failure",
            create_opts_.client_id());
    
    if (subscribe_failure_cb_)
        subscribe_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_subscribe_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_subscribe_success5",
            create_opts_.client_id());
    
    if (subscribe_success_cb_)
        subscribe_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_subscribe_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_subscribe_failure5",
            create_opts_.client_id());
    
    if (subscribe_failure_cb_)
        subscribe_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_unsubscribe_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_unsubscribe_success",
            create_opts_.client_id());
    
    if (unsubscribe_success_cb_)
        unsubscribe_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_unsubscribe_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_unsubscribe_failure",
            create_opts_.client_id());
    
    if (unsubscribe_failure_cb_)
        unsubscribe_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_unsubscribe_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_unsubscribe_success5",
            create_opts_.client_id());
    
    if (unsubscribe_success_cb_)
        unsubscribe_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_unsubscribe_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_unsubscribe_failure5",
            create_opts_.client_id());
    
    if (unsubscribe_failure_cb_)
        unsubscribe_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_publish_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_publish_success",
            create_opts_.client_id());
    
    if (publish_success_cb_)
        publish_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_publish_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_publish_failure",
            create_opts_.client_id());
    
    if (publish_failure_cb_)
        publish_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_publish_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_publish_success5",
            create_opts_.client_id());
    
    if (publish_success_cb_)
        publish_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_publish_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_publish_failure5",
            create_opts_.client_id());
    
    if (publish_failure_cb_)
        publish_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

inline int uvbasic_client::on_ssl_error(const char* _str, size_t _len)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_ssl_error",
            create_opts_.client_id());
    
    if (ssl_error_cb_)
        return ssl_error_cb_(weak_from_this(), _str, _len);
    return 1;
}

inline unsigned int uvbasic_client::on_ssl_psk(const char* _hint, char* _identity, unsigned int _max_identity_len, unsigned char* _psk, unsigned int _max_psk_len)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_ssl_psk",
            create_opts_.client_id());
    
    if (ssl_psk_cb_)
        return ssl_psk_cb_(weak_from_this(), _hint, _identity, _max_identity_len, _psk, _max_psk_len);
    return 1;
}

inline void uvbasic_client::on_destroy_async_call(uv_async_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_destroy_async_call",
            create_opts_.client_id());

    uv_close(reinterpret_cast<uv_handle_t*>(_handle), __on_destroy_async_close);

    if (retry_connect_timer_) {
        uv_timer_stop(retry_connect_timer_.get());
        uv_close(reinterpret_cast<uv_handle_t*>(retry_connect_timer_.get()), __on_retry_connect_timer_close);
    }

    if (health_check_timer_) {
        uv_timer_stop(health_check_timer_.get());
        uv_close(reinterpret_cast<uv_handle_t*>(health_check_timer_.get()), __on_health_check_timer_close);
    }

    std::unique_lock<std::mutex> locker(mtx_);
    if (retry_connect_async_req_)
    {
        uv_close(reinterpret_cast<uv_handle_t*>(retry_connect_async_req_.get()), __on_retry_connect_async_close);
    }
    
    if (retry_connect_async_cancel_)
    {
        uv_close(reinterpret_cast<uv_handle_t*>(retry_connect_async_cancel_.get()), __on_retry_connect_cancel_close);
    }

    //if (MQTTAsync_isConnected(native_cli_)) {
        //MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
        //opts.context = this;
        //opts.onSuccess;
        //opts.onFailure;
        //opts.onSuccess5;
        //opts.onFailure5;
        //opts.timeout = 1000;
        //MQTTAsync_disconnect(native_cli_, &opts);
    //}
}

inline void uvbasic_client::on_destroy_async_close(uv_handle_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_destroy_async_close",
            create_opts_.client_id());

    std::unique_lock<std::mutex> locker(mtx_);
    destroy_async_req_.reset();
    locker.unlock();

    auto self = self_;
    
    handle_counter_.decrement(nullmtx_);
}

inline void uvbasic_client::on_retry_connect_async_call (uv_async_t* _handle)
{
    if (retry_connect_timer_)
    {
        retry_connect_backoff_ms_.store(1000, std::memory_order_release);
        uv_timer_start(retry_connect_timer_.get(), __on_retry_connect_timer_call,
            retry_connect_backoff_ms_.load(std::memory_order_acquire), 0);
    }
    else {
        __set_auto_reconn_hdl_running_mt(false);
    }
}

inline void uvbasic_client::on_retry_connect_async_close(uv_handle_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_retry_connect_async_close",
            create_opts_.client_id());

    std::unique_lock<std::mutex> locker(mtx_);
    retry_connect_async_req_.reset();
    locker.unlock();

    auto self = self_;
    
    handle_counter_.decrement(nullmtx_);
}

inline void uvbasic_client::on_retry_connect_cancel_call(uv_async_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_retry_connect_cancel_call",
            create_opts_.client_id());

    // Stop both timers on the event-loop thread — uv_timer_stop is not
    // thread-safe (per libuv design.rst), and this callback runs on the
    // event-loop thread via uv_async_send from disconnect().
    if (retry_connect_timer_)
        uv_timer_stop(retry_connect_timer_.get());
    if (health_check_timer_)
        uv_timer_stop(health_check_timer_.get());
    __set_auto_reconn_hdl_running_mt(false);
}

inline void uvbasic_client::on_retry_connect_cancel_close(uv_handle_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_retry_connect_cancel_close",
            create_opts_.client_id());

    std::unique_lock<std::mutex> locker(mtx_);
    retry_connect_async_cancel_.reset();
    locker.unlock();

    auto self = self_;
    
    handle_counter_.decrement(nullmtx_);
}

inline void uvbasic_client::on_retry_connect_timer_call (uv_timer_t* _handle)
{
    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        uv_timer_stop(_handle);
        __set_auto_reconn_hdl_running_st(false);
        return;
    }

    std::unique_lock<std::mutex> locker(mtx_);
    auto cleanup = megopp::util::scope_cleanup__create([&] 
    {
        uv_timer_stop(_handle);
        if (!locker.owns_lock())
            locker.lock();
        __set_auto_reconn_hdl_running_st(false);
    });

    if (!destroy_async_req_)
        return;
    if (!native_cli_)
        return;

    if (MQTTAsync_isConnected(native_cli_))
        return;

    if (!__auto_reconn_hdl_running_st())
        return;
    locker.unlock();

    auto e = __connect_mt();
    if (e) {
        cleanup.cancel();
        // Exponential backoff: double the interval, capped at 16000ms
        int cur = retry_connect_backoff_ms_.load(std::memory_order_acquire);
        int next = cur * 2;
        if (next > 16000) next = 16000;
        retry_connect_backoff_ms_.store(next, std::memory_order_release);
        uv_timer_start(_handle, __on_retry_connect_timer_call, next, 0);
        if (log_lvl_ <= log_level::trace)
            _log(log_level::trace, "uvbasic_client({})::on_retry_connect_timer_call; connect failed; code= {}; desc= {}; next retry in {}ms",
                create_opts_.client_id(), e.usercode(), e.message(), next);
    }
}

inline void uvbasic_client::on_retry_connect_timer_close(uv_handle_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_retry_connect_timer_close",
            create_opts_.client_id());

    std::unique_lock<std::mutex> locker(mtx_);
    retry_connect_timer_.reset();
    locker.unlock();

    auto self = self_;
    
    handle_counter_.decrement(nullmtx_);
}

inline void uvbasic_client::on_health_check_timer_call(uv_timer_t* _handle)
{
    // Stop if a user-requested disconnect is in progress
    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        uv_timer_stop(_handle);
        return;
    }

    // Stop if reconnect has already succeeded (on_connected cleared the flag)
    if (!wait_conn_restored_.load(std::memory_order_acquire))
    {
        uv_timer_stop(_handle);
        return;
    }

    // Plan D: Periodic health check — detect if Paho reconnected without
    // the wrapper noticing (e.g. callback race or internal state mismatch).
    {
        std::unique_lock<std::mutex> locker(mtx_);
        if (native_cli_ && MQTTAsync_isConnected(native_cli_))
        {
            // Paho says we're connected — fix wrapper state
            wait_conn_restored_.store(false, std::memory_order_release);
            if (connect_status_.value.load(std::memory_order_acquire) == connect_status::connecting)
                connect_status_.value.store(connect_status::connected, std::memory_order_release);
            uv_timer_stop(_handle);
            locker.unlock();

            if (log_lvl_ <= log_level::trace)
                _log(log_level::trace,
                    "uvbasic_client({})::health_check_timer: detected Paho reconnect, fixing state",
                    create_opts_.client_id());

            if (reconnected_cb_)
                reconnected_cb_(weak_from_this());
            if (connected_cb_)
                connected_cb_(weak_from_this(), (char*)"health check detected reconnect");
            return;
        }
    }

    // Plan B: Periodic reminder — notify the upper layer about ongoing reconnect
    if (reconnect_stalled_cb_)
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - reconnect_start_time_).count();
        reconnect_stalled_cb_(weak_from_this(), elapsed);
    }

    // Restart for next tick
    uv_timer_start(_handle, __on_health_check_timer_call,
                   health_check_interval_sec_.load(std::memory_order_acquire) * 1000, 0);
}

inline void uvbasic_client::on_health_check_timer_close(uv_handle_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_health_check_timer_close",
            create_opts_.client_id());

    std::unique_lock<std::mutex> locker(mtx_);
    health_check_timer_.reset();
    locker.unlock();

    auto self = self_;

    handle_counter_.decrement(nullmtx_);
}

inline void uvbasic_client::on_destroy()
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_destroy",
            create_opts_.client_id());
    
    auto self = self_;
    MEGOPP_UTIL__ON_SCOPE_CLEANUP([this] {
        self_.reset();
    });

    MQTTAsync hdl = nullptr;
    {
        std::unique_lock<std::mutex> locker(mtx_);
        if (native_cli_) {
            hdl = native_cli_;
            native_cli_ = nullptr;
        }
    }
    if (hdl) {
        // MQTTAsync_destroy internally calls MQTTAsync_closeSession,
        // which synchronously sends a DISCONNECT packet if connected,
        // then closes the socket and frees all resources.
        MQTTAsync_destroy(&hdl);
    }
    
}

inline mgpp::err uvbasic_client::__connect_mt()
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto hdl = native_cli_;
    if (!hdl)
        return mgpp::err{ MGEC__ALREADY, "already disconnected" };
    locker.unlock();

    if (disconnect_requested_.load(std::memory_order_acquire))
        return mgpp::err{ MGEC__INPROGRESS, "disconnect in progress" };

    if (connect_status_.value.load(std::memory_order_acquire) != connect_status::disconnected)
        return mgpp::err{ MGEC__ALREADY, "already connected or connecting" };
    connect_status_.value.store(connect_status::connecting, std::memory_order_release);

    int rc = 0;
    if ((rc = MQTTAsync_connect(hdl, &conn_opts_.raw())) != MQTTASYNC_SUCCESS)
    {
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
        // Bug fix: if disconnect() was called while connecting, the flag was set
        // and the caller of disconnect() returned OK expecting a future callback.
        // Since MQTTAsync_connect failed synchronously, no async callback will fire,
        // so we must clear the flag here to prevent permanent deadlock of connect().
        if (disconnect_requested_.load(std::memory_order_acquire))
            disconnect_requested_.store(false, std::memory_order_release);
        return mgpp::err{ MGEC__ERR, rc, "'MQTTAsync_connect' function failed" };
    }

    return {};
}

inline mgpp::err uvbasic_client::__disconnect_mt()
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto hdl = native_cli_;
    if (hdl == nullptr)
        return mgpp::err{ MGEC__ALREADY, "already disconnected" };
    locker.unlock();

    if (connect_status_.value.load(std::memory_order_acquire) != connect_status::connected)
        return mgpp::err{ MGEC__ALREADY, "already disconnected" };
    connect_status_.value.store(connect_status::disconnecting, std::memory_order_release);

    int rc = 0;
    if ((rc = MQTTAsync_disconnect(hdl, &disconn_opts_.raw())) != MQTTASYNC_SUCCESS)
    {
        connect_status_.value.store(connect_status::connected, std::memory_order_release);
        // Bug fix: if MQTTAsync_disconnect fails synchronously (e.g. Paho reports
        // MQTTASYNC_DISCONNECTED because m->c->connected==0), clear the flag so
        // connect() is not permanently blocked.  The caller can retry disconnect()
        // or proceed with a new connect().
        disconnect_requested_.store(false, std::memory_order_release);
        return mgpp::err{ MGEC__ERR, rc, "'MQTTAsync_disconnect' function failed" };
    }

    return {};
}

//inline mgpp::err uvbasic_client::__set_auto_reconnect(bool _b)
//{
//    auto_reconnect_ = _b;
//    return {};
//}

inline outcome::checked<std::shared_ptr<uvbasic_client>, mgpp::err>
    uvbasic_client::create(
        const create_options& _opts,
        const connect_options& _conn_opts,
        const disconnect_options& _disconn_opts, 
        uv_loop_t* _loop)
{
    create_native_options opts;
    opts.assign(_opts);
    std::shared_ptr<uvbasic_client> cli(new uvbasic_client(opts));
    auto e = cli->set_conn_opts(_conn_opts);
    if (e) {
        return outcome::failure(e);
    }
    e = cli->set_disconn_opts(_disconn_opts);
    if (e) {
        return outcome::failure(e);
    }
    e = cli->init(_loop);
    if (e) {
        return outcome::failure(e);
    }
    return outcome::success(cli);
}

inline int uvbasic_client::__on_message_arrived(void* _context, char* _topic_name, int _topic_len, MQTTAsync_message* _message)
{
    return reinterpret_cast<uvbasic_client*>(_context)->on_message_arrived(_topic_name, _topic_len, _message);
}

inline void uvbasic_client::__on_delivery_complete(void* _context, MQTTAsync_token _token)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_delivery_complete(_token);
}

inline void uvbasic_client::__on_connect_lost(void *_context, char *_cause)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_lost(_cause);
}

inline void uvbasic_client::__on_connected(void* _context, char* _cause)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connected(_cause);
}

inline void uvbasic_client::__on_disconnected(void* _context, MQTTProperties* _response, enum MQTTReasonCodes _reason)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnected(_response, _reason);
}

inline void uvbasic_client::__on_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_success(_response);
}

inline void uvbasic_client::__on_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_failure(_response);
}

inline void uvbasic_client::__on_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_success5(_response);
}

inline void uvbasic_client::__on_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_failure5(_response);
}

inline void uvbasic_client::__on_connect_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_success(_response);
}

inline void uvbasic_client::__on_connect_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_failure(_response);
}

inline void uvbasic_client::__on_connect_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_success5(_response);
}

inline void uvbasic_client::__on_connect_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_failure5(_response);
}

inline void uvbasic_client::__on_disconnect_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnect_success(_response);
}

inline void uvbasic_client::__on_disconnect_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnect_failure(_response);
}

inline void uvbasic_client::__on_disconnect_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnect_success5(_response);
}

inline void uvbasic_client::__on_disconnect_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnect_failure5(_response);
}

inline void uvbasic_client::__on_subscribe_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_subscribe_success(_response);
}

inline void uvbasic_client::__on_subscribe_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_subscribe_failure(_response);
}

inline void uvbasic_client::__on_subscribe_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_subscribe_success5(_response);
}

inline void uvbasic_client::__on_subscribe_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_subscribe_failure5(_response);
}

inline void uvbasic_client::__on_unsubscribe_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_unsubscribe_success(_response);
}

inline void uvbasic_client::__on_unsubscribe_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_unsubscribe_failure(_response);
}

inline void uvbasic_client::__on_unsubscribe_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_unsubscribe_success5(_response);
}

inline void uvbasic_client::__on_unsubscribe_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_unsubscribe_failure5(_response);
}

inline void uvbasic_client::__on_publish_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_publish_success(_response);
}

inline void uvbasic_client::__on_publish_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_publish_failure(_response);
}

inline void uvbasic_client::__on_publish_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_publish_success5(_response);
}

inline void uvbasic_client::__on_publish_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_publish_failure5(_response);
}

inline int uvbasic_client::__on_ssl_error(const char* _str, size_t _len, void* _context)
{
    return reinterpret_cast<uvbasic_client*>(_context)->on_ssl_error(_str, _len);
}

inline unsigned int uvbasic_client::__on_ssl_psk(const char* _hint, char* _identity, unsigned int _max_identity_len, unsigned char* _psk, unsigned int _max_psk_len, void* _context)
{
    return reinterpret_cast<uvbasic_client*>(_context)->on_ssl_psk(_hint, _identity, _max_identity_len, _psk, _max_psk_len);
}
    

inline void uvbasic_client::__on_destroy_async_call(uv_async_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_destroy_async_call(_handle);
}

inline void uvbasic_client::__on_destroy_async_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_destroy_async_close(_handle);
}

inline void uvbasic_client::__on_retry_connect_async_call(uv_async_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_retry_connect_async_call(_handle);
}

inline void uvbasic_client::__on_retry_connect_async_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_retry_connect_async_close(_handle);
}


inline void uvbasic_client::__on_retry_connect_cancel_call (uv_async_t * _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_retry_connect_cancel_call(_handle);
}

inline void uvbasic_client::__on_retry_connect_cancel_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_retry_connect_cancel_close(_handle);
}

inline void uvbasic_client::__on_retry_connect_timer_call(uv_timer_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_retry_connect_timer_call(_handle);
}

inline void uvbasic_client::__on_retry_connect_timer_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_retry_connect_timer_close(_handle);
}

inline void uvbasic_client::__on_health_check_timer_call(uv_timer_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_health_check_timer_call(_handle);
}

inline void uvbasic_client::__on_health_check_timer_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_health_check_timer_close(_handle);
}

}
}
}

#endif // !MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED
