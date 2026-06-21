
/**
 * @file async_client.hpp
 * @brief Single-header implementation of mmbkpp::paho_mqtt::async::uvbasic_client.
 *
 * Wraps Eclipse Paho MQTT C asynchronous client (MQTTAsync_* API) with:
 * - libuv event-loop integration (timers, async handles)
 * - Thread-safe connection state machine
 * - Dual-layer automatic reconnection (Paho built-in + wrapper retry timer)
 * - Health-check timer for silent-reconnect detection (Plans B + D)
 *
 * @see MQTTAsync.h (Paho), uv.h (libuv)
 */

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

/**
 * @brief libuv-based asynchronous MQTT client wrapping Eclipse Paho MQTT C library.
 *
 * @class uvbasic_client
 *
 * Provides a complete MQTT client lifecycle (create, connect, publish/subscribe,
 * disconnect, destroy) integrated with a libuv event loop.  Manages two layers of
 * automatic reconnection:
 * - **Paho built-in automaticReconnect**: controlled via MQTTAsync_connectOptions::
 *   automaticReconnect.  After a TCP drop or server DISCONNECT, Paho internally
 *   retries with exponential backoff.
 * - **Wrapper-level retry timer**: activated when MQTTAsync_connect() fails
 *   synchronously during the initial connect() call.  Uses a libuv timer with its
 *   own exponential backoff (1s, 2s, 4s, ..., 16s cap).
 *
 * ## Connection state machine
 *
 * The client maintains a four-state machine via connect_status::value
 * (std::atomic_int):
 *   - disconnected (0)  -- idle, ready to connect.
 *   - connecting   (1)  -- a connect is in flight (Paho or wrapper timer).
 *   - connected    (2)  -- TCP/MQTT session established.
 *   - disconnecting(3)  -- user-requested disconnect in progress.
 *
 * ## Key atomic flags
 *   - disconnect_requested_: set by disconnect(), checked by every async callback
 *     to alter behaviour.
 *   - wait_conn_restored_: distinguishes first-connect from reconnection in
 *     on_connected().
 *   - auto_reconn_hdl_running_: guards the wrapper-layer retry timer.
 *
 * ## Thread safety
 *   - mtx_ serialises access to native_cli_ and the uv handle unique_ptrs.
 *   - uv_timer_stop is NOT thread-safe per libuv design.rst.  disconnect() uses
 *     uv_async_send(cancel_handle) to delegate timer stops to the event-loop thread.
 *   - Paho callbacks run on Paho internal threads; they capture native_cli_ under
 *     mtx_ before use to avoid races with on_destroy().
 *
 * ## Health check timer (Plans B + D)
 *   When Paho automaticReconnect is silently retrying (onFailure nulled after first
 *   call), a periodic uv_timer_t (default 10s) provides:
 *   - **Plan D**: polls MQTTAsync_isConnected() to detect missed callbacks.
 *   - **Plan B**: calls reconnect_stalled_cb_ with elapsed seconds for upper-layer
 *     decisions.
 *
 * @see docs/paho_mqtt_async_uvbasic_client_disconnect_flow.md
 */

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

/**
 * @brief Returns a const reference to the native create options.
 * @return const create_native_options&
 */
    inline constexpr const create_native_options& create_opts() const noexcept { return create_opts_; }
/**
 * @brief Returns a const reference to the native connect options.
 *
 * @warning The raw Paho struct (conn_opts_.raw()) is accessed without mtx_.
 *          This is safe only as long as set_conn_opts() guards against
 *          modification while connected or while auto-reconnect is running.
 * @return const connect_native_options&
 */
    inline constexpr const connect_native_options& connect_opts() const noexcept { return conn_opts_; }
/**
 * @brief Returns a const reference to the native disconnect options.
 * @warning Same caveat as connect_opts().
 * @return const disconnect_native_options&
 */
    inline constexpr const disconnect_native_options& disconnect_opts() const noexcept { return disconn_opts_; }

/**
 * @brief Returns the raw Paho MQTTAsync handle WITHOUT locking.
 *
 * @warning Only use from the event-loop thread or when you can guarantee
 *          no concurrent on_destroy().  Prefer native_mt() for cross-thread
 *          access.
 * @return MQTTAsync -- the native Paho handle, or nullptr if destroyed.
 */
    inline MQTTAsync native_st() const noexcept { return native_cli_; }
/**
 * @brief Returns the raw Paho MQTTAsync handle WITH lock held.
 *
 * Holds mtx_ during the read of native_cli_, preventing races with
 * on_destroy().  The caller receives a copy of the pointer; the Paho
 * handle itself may still be destroyed asynchronously after the lock
 * is released.
 *
 * @return MQTTAsync -- the native Paho handle, or nullptr if destroyed.
 * @note Thread-safe.
 */
    inline MQTTAsync native_mt() const
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return native_cli_;
    }
    
/**
 * @brief Queries whether the client is currently connected.
 *
 * Uses native_mt() (lock-protected) to safely read native_cli_, then calls
 * MQTTAsync_isConnected() which internally holds Paho global mutex.
 *
 * @return true if native_cli_ is non-null AND Paho reports connected.
 * @note Thread-safe.
 */
    inline bool is_connected() const noexcept { auto hdl = native_mt(); return hdl && MQTTAsync_isConnected(hdl); }

    /**
     * @brief CR-1 fix: queries MQTTAsync_isConnected() WITHOUT holding mtx_.
     *
     * Reads native_cli_ under mtx_, copies the handle, then releases the lock
     * BEFORE calling the Paho API.  This avoids an ABBA deadlock between our
     * mtx_ and Paho's internal mqttasync_mutex:
     *
     *   T1 (Paho receive thread): holds mqttasync_mutex → callback → tries mtx_
     *   T2 (libuv/user thread):    holds mtx_ → MQTTAsync_isConnected → tries mqttasync_mutex
     *
     * @return true if connected (native_cli_ non-null and Paho reports connected).
     * @note  Thread-safe.  Safe to call from any thread.
     */
    inline bool __is_connected_safe() const noexcept
    {
        MQTTAsync hdl = nullptr;
        {
            std::lock_guard<std::mutex> locker(mtx_);
            hdl = native_cli_;
        }
        return hdl && MQTTAsync_isConnected(hdl);
    }

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
    // Reconnect start epoch in steady-clock milliseconds.
    // Written from Paho threads, read from libuv event-loop thread;
    // std::atomic prevents formal data-race UB (see NEW-1).
    std::atomic_int_fast64_t reconnect_start_time_ms_{0};
    std::atomic_int health_check_interval_sec_{10};

    std::atomic_int retry_connect_backoff_ms_{1000};

    std::atomic_bool health_check_timer_needs_start_{false};
    // Set to true in on_destroy_async_call() before any uv_close(); all
    // uv_async_send call sites must check this flag to avoid UB on a
    // closing handle (F-01 fix).
    std::atomic_bool destroying_{false};
    
};

/**
 * @brief Private constructor -- use uvbasic_client::create() instead.
 *
 * Initialises the connection state to disconnected, wires the Paho callback
 * pointers (onSuccess/onFailure/onSuccess5/onFailure5) into the connect and
 * disconnect options structs, and sets up the handle_counter_ to trigger
 * on_destroy() when all libuv handles have been closed.
 *
 * @param _opts  The native create options (client ID, persistence type, MQTT
 *               version).
 *
 * @note The _opts.raw().MQTTVersion is used to initialise conn_opts_ and
 *       disconn_opts_ with the correct struct_version for MQTT v3 vs v5.
 */
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

/**
 * @brief Destructor -- empty by design.
 *
 * All resource cleanup is handled asynchronously through the handle_counter_ /
 * on_destroy() mechanism.  The destructor itself does nothing because the object
 * may still have outstanding libuv handles being closed.
 *
 * @see destroy_request(), on_destroy()
 */
uvbasic_client::~uvbasic_client()
{
}

/**
 * @brief Sets the MQTT connect options.
 *
 * Copies the user-provided options into the native Paho struct (conn_opts_).
 * The copy is performed under mtx_ and guarded against modification while
 * connected or while the wrapper auto-reconnect timer is running.
 *
 * @param _opts  The new connect options.
 * @return mgpp::err -- OK on success.
 * @retval MGEC__PERM  Already connected, or auto-reconnect is running.
 *
 * @note Thread-safe.
 */
inline mgpp::err uvbasic_client::set_conn_opts(const connect_options& _opts)
{
    // CR-1 fix: check connected OUTSIDE mtx_ to avoid ABBA deadlock.
    // __is_connected_safe() copies native_cli_ under mtx_, releases the lock,
    // then calls MQTTAsync_isConnected() — so Paho's internal mqttasync_mutex
    // is never contended with our mtx_ in opposite order.
    if (__is_connected_safe())
        return mgpp::err{ MGEC__PERM, "already connected" };
    
    if (__auto_reconn_hdl_running_mt())
        return mgpp::err{ MGEC__PERM, "auto reconnect is running" };

    // HI-1 fix: refuse modification while a connection attempt (including
    // Paho's internal automaticReconnect) is in flight.  When Paho calls
    // MQTTAsync_connect(), it stores our conn_opts_ raw pointer in its
    // internal m->connect struct (shallow copy).  The ssl field in that
    // struct points directly to ssl_opt_ inside conn_opts_.  On every
    // auto-reconnect retry, Paho dereferences m->connect.ssl to deep-copy
    // the SSL strings via MQTTStrdup() (MQTTAsync.c line ~800).
    //
    // If we call conn_opts_.assign() here — which may destroy the old
    // ssl_opt_ unique_ptr — Paho's next retry reads freed memory:
    //
    //   m->connect.ssl  →  ssl_opt_.get()  →  [freed]  ← UAF crash
    //
    // Timeline:
    //   T1  connect(SSL_A)               → Paho saves m->connect.ssl → &SSL_A
    //   T2  network drops               → Paho starts auto-reconnect
    //   T3  set_conn_opts(SSL_B)         → conn_opts_.assign() frees SSL_A
    //   T4  Paho retries                 → reads m->connect.ssl → UAF
    //
    // connect_status_ transitions to connecting inside on_connect_lost
    // (line ~1769) when automaticReconnect is nonzero, which covers both
    // Paho internal and wrapper-driven reconnect.  Reading it without mtx_
    // is safe because connect_status_.value is std::atomic.
    if (connect_status_.value.load(std::memory_order_acquire) != connect_status::disconnected)
        return mgpp::err{ MGEC__PERM,
            "connect options cannot be modified while connecting; "
            "disconnect and wait for the callback before changing options" };

    std::unique_lock<std::mutex> locker(mtx_);
    conn_opts_.assign(_opts);
    
    // F-N2 fix: Re-inject SSL error/PSK callbacks that may have been
    // lost if set_conn_opts() created a new ssl_native_options internally
    // (e.g. when the previous connection had no SSL configured).
    // ssl_native_options::assign() copies user-visible fields (trustStore,
    // keyStore, etc.) but does NOT copy ssl_error_cb, ssl_error_context,
    // ssl_psk_cb, or ssl_psk_context — losing the callbacks registered
    // by init().  Re-set them here so SSL error/PSK notifications are
    // never silently dropped.
    if (conn_opts_.ssl())
    {
        conn_opts_.ssl()->raw().ssl_error_cb = __on_ssl_error;
        conn_opts_.ssl()->raw().ssl_error_context = this;
        conn_opts_.ssl()->raw().ssl_psk_cb = __on_ssl_psk;
        conn_opts_.ssl()->raw().ssl_psk_context = this;
    }
    
    return {};
}

/**
 * @brief Sets the MQTT disconnect options.
 *
 * Copies the user-provided options into the native Paho struct (disconn_opts_).
 * The copy is performed under mtx_ and guarded against modification while
 * connected or while the wrapper auto-reconnect timer is running.
 *
 * @param _opts  The new disconnect options.
 * @return mgpp::err -- OK on success.
 * @retval MGEC__PERM  Already connected, or auto-reconnect is running.
 *
 * @note Sets raw_disconn_opt_.struct_version = 1 so that Paho reads reasonCode
 *       for MQTT v5 connections.
 * @note Thread-safe.
 */
inline mgpp::err uvbasic_client::set_disconn_opts(const disconnect_options& _opts)
{
    // CR-1 fix: check connected OUTSIDE mtx_ (same rationale as set_conn_opts).
    if (__is_connected_safe())
        return mgpp::err{ MGEC__PERM, "already connected" };

    if (__auto_reconn_hdl_running_mt())
        return mgpp::err{ MGEC__PERM, "auto reconnect is running" };

    std::unique_lock<std::mutex> locker(mtx_);
    disconn_opts_.assign(_opts);
    return {};
}

/**
 * @brief Initiates a connection to the MQTT broker.
 *
 * ## Pre-condition checks (under mtx_):
 *   - destroy_async_req_ must exist (client initialised).
 *   - native_cli_ must be non-null.
 *   - disconnect_requested_ must be false (no pending disconnect).
 *   - MQTTAsync_isConnected() must return false (idempotent).
 *   - The wrapper retry timer must not be running.
 *
 * ## Execution:
 *   1. Resets retry_connect_backoff_ms_ to 1s.
 *   2. Calls __connect_mt() which sets connect_status_ = connecting and calls
 *      MQTTAsync_connect().
 *   3. If __connect_mt() fails synchronously AND Paho automaticReconnect is
 *      enabled, starts the wrapper-layer retry timer via uv_async_send.
 *   4. Returns OK; the actual result arrives asynchronously via
 *      on_connect_success / on_connect_failure (and on_connected).
 *
 * @return mgpp::err -- OK if the connect was initiated, or an error code.
 * @retval MGEC__INVALID_HANDLE  Client not initialised or destroyed.
 * @retval MGEC__INPROGRESS      Disconnect in progress, or retry timer running.
 * @retval MGEC__ALREADY         Already connected.
 * @retval MGEC__ERR             Paho connect API returned a synchronous error.
 *
 * @note Thread-safe: may be called from any thread.
 */
inline mgpp::err uvbasic_client::connect()
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::connect enter | status={} disc_req={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire));
    std::unique_lock<std::mutex> locker(mtx_);
    if (!destroy_async_req_)
        return mgpp::err{ MGEC__INVALID_HANDLE, "invalid handle" };
    if (!native_cli_)
        return mgpp::err{ MGEC__INVALID_HANDLE, "invalid handle" };

    if (disconnect_requested_.load(std::memory_order_acquire))
        return mgpp::err{ MGEC__INPROGRESS, "a disconnect is in progress, retry later" };

    // CR-1 fix: unlock mtx_ BEFORE the Paho isConnected check.  Holding
    // mtx_ across MQTTAsync_isConnected() creates an ABBA deadlock risk
    // with Paho callback threads (see __is_connected_safe() doc).
    // __auto_reconn_hdl_running_mt() reads an atomic bool so it does not
    // require mtx_ protection.
    locker.unlock();

    if (__is_connected_safe())
        return {};
    
    if (__auto_reconn_hdl_running_mt())
        return mgpp::err{ MGEC__INPROGRESS, "auto reconnect is running" };
    
    // Reset backoff and reconnect-wait flag for fresh user-initiated connect.
    // Clearing wait_conn_restored_ prevents a spurious reconnected_cb_ when
    // a prior connection's on_connect_lost fires on Paho's receive thread
    // between MQTTAsync_connect() success and the CONNACK arrival (BUG 30).
    retry_connect_backoff_ms_.store(1000, std::memory_order_release);
    wait_conn_restored_.store(false, std::memory_order_release);
    
    auto e = __connect_mt();
    if ( e ) {
        // If disconnect() was called concurrently during __connect_mt(),
        // the flag is set and we must not start the retry timer.
        if (disconnect_requested_.load(std::memory_order_acquire))
            return e;
        if (__auto_reconnect_enable()) 
        {
            locker.lock();
            if (retry_connect_async_req_)
            {
                // F-01: if destroy is in progress, the handle may already be
                // closing — uv_async_send on a closing handle is UB.
                if (destroying_.load(std::memory_order_acquire)) {
                    locker.unlock();
                    return e;
                }
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

    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::connect exit | rc=0",
            create_opts_.client_id());
    return {};
}

/**
 * @brief Initiates a graceful disconnect from the MQTT broker.
 *
 * ## Three-phase design:
 *
 * **Phase 1** -- Set flags (atomic, thread-safe):
 *   - disconnect_requested_ = true  (all async callbacks check this first)
 *   - wait_conn_restored_ = false   (cancel any in-progress reconnect wait)
 *
 * **Phase 2** -- Signal event-loop thread to stop timers:
 *   - Sets auto_reconn_hdl_running_ = false (atomic).
 *   - Sends uv_async_send(retry_connect_async_cancel_) so that
 *     on_retry_connect_cancel_call() (running on the event-loop thread) can
 *     safely call uv_timer_stop() on both timers.  This is necessary because
 *     uv_timer_stop() is NOT thread-safe per libuv design document.
 *
 * **Phase 3** -- Route by current connect_status_:
 *   - disconnected:  clear flag, return OK (idempotent).
 *   - disconnecting: return MGEC__ALREADY (already in progress).
 *   - connecting:    return OK without calling Paho; wait for the connect
 *                    callback to handle the disconnect (see on_connect_success).
 *   - connected:     call __disconnect_mt() to MQTTAsync_disconnect().
 *
 * ## Async completion:
 *   - Normal path:   on_disconnect_success/success5 sets disconnected.
 *   - Failure path:  on_disconnect_failure/failure5 defensive cleanup (Paho
 *                    never actually calls onFailure for disconnect currently).
 *
 * @return mgpp::err -- OK if the disconnect was initiated or already complete.
 * @retval MGEC__ALREADY  Disconnect already in progress, or already disconnected.
 * @retval MGEC__ERR      Paho disconnect API returned a synchronous error.
 *
 * @note Thread-safe: may be called from any thread.  The actual uv_timer_stop
 *       calls are safely delegated to the event-loop thread via uv_async_send.
 * @note Idempotent when connect_status_ == disconnected.
 */
inline mgpp::err uvbasic_client::disconnect()
{
    // Phase 1: Set the flag first — all async callbacks check this to change their behaviour
    disconnect_requested_.store(true, std::memory_order_release);
    wait_conn_restored_.store(false, std::memory_order_release);
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::disconnect enter | status={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)));

    // Phase 2: Set the wrapper-layer flag synchronously (atomic, thread-safe),
    // then signal the event-loop thread to stop timers safely.
    // Per libuv docs (design.rst), the event loop and handles are NOT thread-safe
    // except where stated otherwise; uv_async_send is the only API confirmed safe
    // from any thread.
    if (__auto_reconn_hdl_running_mt())
        __set_auto_reconn_hdl_running_st(false);

    {
        // Read retry_connect_async_cancel_ under mtx_ — on_retry_connect_cancel_close
        // may reset() it on the event-loop thread during destroy (see NEW-2).
        std::unique_lock<std::mutex> locker(mtx_);
        if (retry_connect_async_cancel_ &&
            !destroying_.load(std::memory_order_acquire))  // F-01 fix: avoid UB on closing handle
            uv_async_send(retry_connect_async_cancel_.get());
    }

    // Phase 3: Route based on current connection state
    int cur = connect_status_.value.load(std::memory_order_acquire);

    if (cur == connect_status::disconnected)
    {
        // Idempotent — already disconnected
        if (log_lvl_ <= log_level::trace)
            _log(log_level::trace, "uvbasic_client({})::disconnect exit | idempotent (already disconnected)",
                create_opts_.client_id());
        disconnect_requested_.store(false, std::memory_order_release);
        return {};
    }

    if (cur == connect_status::disconnecting)
    {
        // Already in progress
        if (log_lvl_ <= log_level::trace)
            _log(log_level::trace, "uvbasic_client({})::disconnect exit | already in progress",
                create_opts_.client_id());
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

/**
 * @brief Publishes a message to the given topic.
 *
 * Wires the MQTTAsync_responseOptions callbacks (onSuccess/onFailure or
 * onSuccess5/onFailure5 depending on MQTT version) and calls
 * MQTTAsync_sendMessage().
 *
 * @param _destination_name  The MQTT topic to publish to.
 * @param _msg               The MQTT message (payload, QoS, retained flag).
 * @param _opts              Response options; callbacks will be overwritten.
 * @return mgpp::err -- OK on success.
 * @retval MGEC__PERM  Client not created (native_cli_ is null).
 * @retval MGEC__ERR   MQTTAsync_sendMessage() returned an error.
 *
 * @note Thread-safe: captures native_cli_ under mtx_ before the Paho call.
 */
inline mgpp::err uvbasic_client::send_message(const memepp::string& _destination_name, const MQTTAsync_message& _msg, MQTTAsync_responseOptions& _opts)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::send_message | topic={}",
            create_opts_.client_id(), _destination_name.data());
    _opts.context = this;
    // CR-2 fix: use conn_opts_ (actual connection version) instead of
    // create_opts_ (always MQTTVERSION_DEFAULT).  The MQTT protocol version
    // is determined by the connect() call, not by create options.
    if (conn_opts_.raw().MQTTVersion < MQTTVERSION_5)
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

    // Extend the object lifetime so that native_cli_ is not freed by
    // a concurrent on_destroy() between the unlock and the Paho call
    // (F-05 fix).
    auto self = self_;

    int rc = 0;
    if ((rc = MQTTAsync_sendMessage(hdl, _destination_name.data(), &_msg, &_opts)) != MQTTASYNC_SUCCESS)
    {
        return mgpp::err{ MGEC__ERR, rc, "send_message failed" };
    }

    return {};
}

/**
 * @brief Subscribes to a topic with the given QoS.
 *
 * Wires the MQTTAsync_responseOptions callbacks and calls MQTTAsync_subscribe().
 * The result arrives asynchronously via on_subscribe_success/failure.
 *
 * @param _topic  The MQTT topic filter to subscribe to.
 * @param _qos    The requested QoS level (0, 1, or 2).
 * @param _opts   Response options; callbacks will be overwritten.
 * @return mgpp::err -- OK on success.
 * @retval MGEC__PERM  Client not created.
 * @retval MGEC__ERR   MQTTAsync_subscribe() returned an error.
 *
 * @note Thread-safe.
 */
inline mgpp::err uvbasic_client::subscribe(const memepp::string& _topic, int _qos, MQTTAsync_responseOptions& _opts)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::subscribe | topic={} qos={}",
            create_opts_.client_id(), _topic.data(), _qos);
    _opts.context = this;
    // CR-2 fix: use conn_opts_ (actual connection version) instead of
    // create_opts_ (always MQTTVERSION_DEFAULT).  The MQTT protocol version
    // is determined by the connect() call, not by create options.
    if (conn_opts_.raw().MQTTVersion < MQTTVERSION_5)
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

    // Extend the object lifetime so that native_cli_ is not freed by
    // a concurrent on_destroy() between the unlock and the Paho call
    // (F-05 fix).
    auto self = self_;

    int rc = 0;
    if ((rc = MQTTAsync_subscribe(hdl, _topic.data(), _qos, &_opts)) != MQTTASYNC_SUCCESS)
    {
        return mgpp::err{ MGEC__ERR, rc, "subscribe failed" };
    }

    return {};
}

/**
 * @brief Unsubscribes from a topic.
 *
 * Wires the MQTTAsync_responseOptions callbacks and calls MQTTAsync_unsubscribe().
 * The result arrives asynchronously via on_unsubscribe_success/failure.
 *
 * @param _topic  The MQTT topic filter to unsubscribe from.
 * @param _opts   Response options; callbacks will be overwritten.
 * @return mgpp::err -- OK on success.
 * @retval MGEC__PERM  Client not created.
 * @retval MGEC__ERR   MQTTAsync_unsubscribe() returned an error.
 *
 * @note Thread-safe.
 */
inline mgpp::err uvbasic_client::unsubscribe(const memepp::string& _topic, MQTTAsync_responseOptions& _opts)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::unsubscribe | topic={}",
            create_opts_.client_id(), _topic.data());
    _opts.context = this;
    // CR-2 fix: use conn_opts_ (actual connection version) instead of
    // create_opts_ (always MQTTVERSION_DEFAULT).  The MQTT protocol version
    // is determined by the connect() call, not by create options.
    if (conn_opts_.raw().MQTTVersion < MQTTVERSION_5)
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

    // Extend the object lifetime so that native_cli_ is not freed by
    // a concurrent on_destroy() between the unlock and the Paho call
    // (F-05 fix).
    auto self = self_;

    int rc = 0;
    if ((rc = MQTTAsync_unsubscribe(hdl, _topic.data(), &_opts)) != MQTTASYNC_SUCCESS)
    {
        return mgpp::err{ MGEC__ERR, rc, "unsubscribe failed" };
    }

    return {};
}

/**
 * @brief Initialises the Paho MQTT handle and all libuv handles.
 *
 * Calls MQTTAsync_createWithOptions(), registers Paho callbacks (message arrived,
 * connected, disconnected, connection lost), and initialises five libuv handles:
 *   1. destroy_async_req_        -- signals shutdown
 *   2. retry_connect_async_req_  -- starts the wrapper retry timer
 *   3. retry_connect_async_cancel_ -- stops timers on the event-loop thread
 *   4. retry_connect_timer_      -- wrapper-layer retry with exponential backoff
 *   5. health_check_timer_       -- periodic MQTTAsync_isConnected() polling
 *
 * The handle_counter_ is set to 5; each close callback decrements it.
 * When it reaches 0, on_destroy() is invoked to free the Paho handle.
 *
 * @param _loop  The libuv event loop to attach all handles to.
 * @return       mgpp::err -- OK on success, or an error if any Paho API fails.
 *
 * @note Must be called exactly once after construction.  The caller's shared_ptr
 *       is retained in self_ to keep the object alive during async operations.
 * @pre  destroy_async_req_ must be nullptr (not yet initialised).
 * @post native_cli_ is valid; all five libuv handles are initialised and stored.
 */
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

    // MQTTAsync_setCallbacks registers connectionLost, messageArrived,
    // AND deliveryComplete in one call — the two separate set* calls used
    // previously (setMessageArrivedCallback + setConnectionLostCallback)
    // did NOT set deliveryComplete (F-02 fix).
    rc = MQTTAsync_setCallbacks(handle, this, __on_connect_lost,
                                __on_message_arrived, __on_delivery_complete);
    if (rc != MQTTASYNC_SUCCESS) {
        return mgpp::err{ MGEC__ERR, rc, "MQTTAsync_setCallbacks failed" };
    }

    rc = MQTTAsync_setConnected(handle, this, __on_connected);
    if (rc != MQTTASYNC_SUCCESS) {
        return mgpp::err{ MGEC__ERR, rc, "MQTTAsync_setConnected failed" };
    }

    rc = MQTTAsync_setDisconnected(handle, this, __on_disconnected);
    if (rc != MQTTASYNC_SUCCESS) {
        return mgpp::err{ MGEC__ERR, rc, "MQTTAsync_setDisconnected failed" };
    }

    // Wire SSL error and PSK callbacks into Paho's SSL options struct
    // (F-03 fix).  These fields were never set before, so Paho would
    // never invoke the user-installed SSL error/PSK callbacks.
    if (conn_opts_.ssl())
    {
        conn_opts_.ssl()->raw().ssl_error_cb = __on_ssl_error;
        conn_opts_.ssl()->raw().ssl_error_context = this;
        conn_opts_.ssl()->raw().ssl_psk_cb = __on_ssl_psk;
        conn_opts_.ssl()->raw().ssl_psk_context = this;
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

/**
 * @brief Initiates asynchronous destruction of the client.
 *
 * Sends a signal via uv_async_send(destroy_async_req_) to the event-loop thread.
 * The actual teardown sequence is:
 *   1. on_destroy_async_call() closes all libuv handles (timers + async handles).
 *   2. Each close callback calls handle_counter_.decrement().
 *   3. When the counter reaches 0, on_destroy() calls MQTTAsync_destroy().
 *
 * @note MQTTAsync_destroy() internally sends a DISCONNECT packet and closes the
 *       socket synchronously -- no prior disconnect() call is needed.
 * @note Thread-safe: may be called from any thread.
 * @note Idempotent: safe to call multiple times.
 */
inline void uvbasic_client::destroy_request()
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::destroy_request",
            create_opts_.client_id());
    std::unique_lock<std::mutex> locker(mtx_);
    if (!destroy_async_req_)
        return;
    // F-01: after on_destroy_async_call sets destroying_ and calls uv_close(),
    // the handle is closing — uv_async_send on a closing handle is UB.
    if (destroying_.load(std::memory_order_acquire))
        return;
    uv_async_send(destroy_async_req_.get());
}

/**
 * @brief Sets the minimum log level for internal logging.
 *
 * Log messages below this level are suppressed.  Log output is delivered
 * via the log_callback (see set_log_callback()).
 *
 * @param _level  The minimum log level (see log_level enum).
 * @note Not guarded -- can be changed at any time.
 */
inline void uvbasic_client::set_log_level(log_level _level)
{
    log_lvl_ = _level;
}

/**
 * @brief Sets the log output callback.
 *
 * When set, all _log() calls at or above log_lvl_ will invoke this callback
 * with a weak_ptr to the client, the log level, and the formatted message.
 *
 * @param _cb  The log callback.  Pass an empty/default functor to disable logging.
 * @note **Guarded**: NOT set if connected.
 */
inline void uvbasic_client::set_log_callback(const log_callback& _cb)
{
    // CR-1 fix: read native_cli_ under mtx_, then check outside lock
    if (__is_connected_safe())
        return;
    log_cb_ = _cb;
}

/**
 * @brief Sets the message-arrived callback.
 *
 * The callback receives a weak_ptr to this client as its first argument,
 * allowing the user to check liveness before accessing the client.
 *
 * @note **Guarded**: the callback is NOT set if the client is already connected
 *       or if the wrapper-layer auto-reconnect timer is running.  This prevents
 *       mid-session callback changes that could race with Paho internal threads.
 * @param _cb  The callback functor (std::function).  Pass an empty/default
 *             functor to clear.
 */
inline void uvbasic_client::set_message_arrived_callback(const message_arrived_callback& _cb) 
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    message_arrived_cb_ = _cb;
}

/**
 * @brief Sets the delivery-complete callback.
 *
 * The callback receives a weak_ptr to this client as its first argument,
 * allowing the user to check liveness before accessing the client.
 *
 * @note **Guarded**: the callback is NOT set if the client is already connected
 *       or if the wrapper-layer auto-reconnect timer is running.  This prevents
 *       mid-session callback changes that could race with Paho internal threads.
 * @param _cb  The callback functor (std::function).  Pass an empty/default
 *             functor to clear.
 */
inline void uvbasic_client::set_delivery_complete_callback(const delivery_complete_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    delivery_complete_cb_ = _cb;
}

/**
 * @brief Sets the connection-lost callback.
 *
 * The callback receives a weak_ptr to this client as its first argument,
 * allowing the user to check liveness before accessing the client.
 *
 * @note **Guarded**: the callback is NOT set if the client is already connected
 *       or if the wrapper-layer auto-reconnect timer is running.  This prevents
 *       mid-session callback changes that could race with Paho internal threads.
 * @param _cb  The callback functor (std::function).  Pass an empty/default
 *             functor to clear.
 */
inline void uvbasic_client::set_connect_lost_callback(const connect_lost_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    connect_lost_cb_ = _cb;
}

/**
 * @brief Sets the connected (on_connected from Paho) callback.
 *
 * The callback receives a weak_ptr to this client as its first argument,
 * allowing the user to check liveness before accessing the client.
 *
 * @note **Guarded**: the callback is NOT set if the client is already connected
 *       or if the wrapper-layer auto-reconnect timer is running.  This prevents
 *       mid-session callback changes that could race with Paho internal threads.
 * @param _cb  The callback functor (std::function).  Pass an empty/default
 *             functor to clear.
 */
inline void uvbasic_client::set_connected_callback(const connected_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    connected_cb_ = _cb;
}

/**
 * @brief Sets the disconnected (server-initiated DISCONNECT, MQTT v5) callback.
 *
 * The callback receives a weak_ptr to this client as its first argument,
 * allowing the user to check liveness before accessing the client.
 *
 * @note **Guarded**: the callback is NOT set if the client is already connected
 *       or if the wrapper-layer auto-reconnect timer is running.  This prevents
 *       mid-session callback changes that could race with Paho internal threads.
 * @param _cb  The callback functor (std::function).  Pass an empty/default
 *             functor to clear.
 */
inline void uvbasic_client::set_disconnected_callback(const disconnected_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    disconnected_cb_ = _cb;
}

/**
 * @brief Sets the reconnected (Paho auto-reconnect succeeded) callback.
 *
 * The callback receives a weak_ptr to this client as its first argument,
 * allowing the user to check liveness before accessing the client.
 *
 * @note **Guarded**: the callback is NOT set if the client is already connected
 *       or if the wrapper-layer auto-reconnect timer is running.  This prevents
 *       mid-session callback changes that could race with Paho internal threads.
 * @param _cb  The callback functor (std::function).  Pass an empty/default
 *             functor to clear.
 */
inline void uvbasic_client::set_reconnected_callback(const reconnected_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    reconnected_cb_ = _cb;
}

/**
 * @brief Sets the reconnect-stalled (periodic health-check reports) callback.
 *
 * The callback receives a weak_ptr to this client as its first argument,
 * allowing the user to check liveness before accessing the client.
 *
 * @note **Guarded**: the callback is NOT set if the client is already connected
 *       or if the wrapper-layer auto-reconnect timer is running.  This prevents
 *       mid-session callback changes that could race with Paho internal threads.
 * @param _cb  The callback functor (std::function).  Pass an empty/default
 *             functor to clear.
 */
inline void uvbasic_client::set_reconnect_stalled_callback(const reconnect_stalled_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    reconnect_stalled_cb_ = _cb;
}

/**
 * @brief Sets the health-check timer interval.
 *
 * Controls how often the health-check timer polls MQTTAsync_isConnected()
 * and calls reconnect_stalled_cb_ during silent Paho auto-reconnect.
 *
 * @param _seconds  Interval in seconds.  Clamped to [1, 3600].  Default: 10.
 * @note Not guarded -- can be changed while the timer is running.
 *       The new value takes effect on the next timer tick.
 */
inline void uvbasic_client::set_health_check_interval(int _seconds)
{
    if (_seconds < 1)
        _seconds = 1;
    if (_seconds > 3600)
        _seconds = 3600;
    health_check_interval_sec_.store(_seconds, std::memory_order_release);
}

/**
 * @brief Sets the generic success callback (shared ownership variant).
 *
 * Unlike other callbacks stored as plain std::function, this one is stored as
 * std::shared_ptr<std::function<...>> to allow sharing between multiple observers.
 *
 * @note Can be set at any time (no connected/running guards).
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_success_callback(const success_callback& _cb)
{
    std::shared_ptr<success_callback> cb;
    if (_cb) {
        cb = std::make_shared<success_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    success_cb_ = cb;
}

/**
 * @brief Sets the generic failure callback (shared ownership variant).
 * @note Can be set at any time (no connected/running guards).
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_failure_callback(const failure_callback& _cb)
{
    std::shared_ptr<failure_callback> cb;
    if (_cb) {
        cb = std::make_shared<failure_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    failure_cb_ = cb;
}

/**
 * @brief Sets the connect-success callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_connect_success_callback(const connect_success_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    connect_success_cb_ = _cb;
}

/**
 * @brief Sets the connect-failure callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_connect_failure_callback(const connect_failure_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    connect_failure_cb_ = _cb;
}

/**
 * @brief Sets the disconnect-success callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_disconnect_success_callback(const disconnect_success_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    disconnect_success_cb_ = _cb;
}

/**
 * @brief Sets the disconnect-failure callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_disconnect_failure_callback(const disconnect_failure_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    disconnect_failure_cb_ = _cb;
}

/**
 * @brief Sets the subscribe-success callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_subscribe_success_callback(const subscribe_success_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    subscribe_success_cb_ = _cb;
}

/**
 * @brief Sets the subscribe-failure callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_subscribe_failure_callback(const subscribe_failure_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    subscribe_failure_cb_ = _cb;
}

/**
 * @brief Sets the unsubscribe-success callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_unsubscribe_success_callback(const unsubscribe_success_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    unsubscribe_success_cb_ = _cb;
}

/**
 * @brief Sets the unsubscribe-failure callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_unsubscribe_failure_callback(const unsubscribe_failure_callback& _cb)
{
    // CR-1 fix: use __is_connected_safe() which releases mtx_ before
    // calling MQTTAsync_isConnected(), avoiding ABBA deadlock.
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    unsubscribe_failure_cb_ = _cb;
}

/**
 * @brief Sets the publish-success callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_publish_success_callback(const publish_success_callback& _cb)
{
    // CR-1 fix: read native_cli_ under mtx_, then check outside lock
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    publish_success_cb_ = _cb;
}

/**
 * @brief Sets the publish-failure callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_publish_failure_callback(const publish_failure_callback& _cb)
{
    // CR-1 fix: read native_cli_ under mtx_, then check outside lock
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    publish_failure_cb_ = _cb;
}

/**
 * @brief Sets the SSL-error callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_ssl_error_callback(const ssl_error_callback& _cb)
{
    // CR-1 fix: read native_cli_ under mtx_, then check outside lock
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    ssl_error_cb_ = _cb;
}

/**
 * @brief Sets the SSL-PSK callback.
 *
 * The callback receives a weak_ptr to this client as its first argument.
 *
 * @note **Guarded**: NOT set if connected or auto-reconnect is running.
 * @param _cb  The callback functor.
 */
inline void uvbasic_client::set_ssl_psk_callback(const ssl_psk_callback& _cb)
{
    // CR-1 fix: read native_cli_ under mtx_, then check outside lock
    if (__is_connected_safe())
        return;

    if (__auto_reconn_hdl_running_mt())
        return;
    ssl_psk_cb_ = _cb;
}

/**
 * @brief Paho callback: a message has arrived on a subscribed topic.
 *
 * Runs on Paho internal receive thread.  Calls the user message_arrived_cb_.
 * The message and topic memory are owned by Paho and must be freed unless the
 * callback returns 0 (meaning "I took ownership").
 *
 * @param _topic_name  The topic string (Paho-allocated).
 * @param _topic_len   Length of the topic string.
 * @param _message     The MQTT message (payload, QoS, etc.).
 * @return 0 if the callback took ownership of _topic_name and _message
 *         (they will NOT be freed), or 1 if Paho should free them.
 *
 * @note Thread: Paho internal receive thread.
 */
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

/**
 * @brief Paho callback: a QoS 1/2 message delivery has completed.
 *
 * @param _token  The delivery token assigned when the message was published.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_delivery_complete(MQTTAsync_token _token)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_delivery_complete: token=%d", 
            create_opts_.client_id(), static_cast<int>(_token));
    
    if (delivery_complete_cb_)
        delivery_complete_cb_(weak_from_this(), _token);
}

/**
 * @brief Paho callback: the TCP/MQTT connection has been lost.
 *
 * ## Triggered by:
 *   - TCP socket error / drop.
 *   - Server DISCONNECT packet (after on_disconnected, via nextOrClose).
 *   - Connect timeout (via nextOrClose).
 *
 * ## Behavior:
 *   - If disconnect_requested_ is true: only notifies the user callback,
 *     does NOT start reconnection.
 *   - If Paho automaticReconnect is enabled: sets wait_conn_restored_ = true,
 *     connect_status_ = connecting, and starts the health-check timer.
 *     Paho startConnectRetry will handle the actual reconnection.
 *   - If Paho automaticReconnect is disabled: sets connect_status_ = disconnected.
 *
 * @param _cause  Human-readable reason string from Paho (may be NULL).
 * @note Thread: Paho internal thread (via nextOrClose or socket error path).
 * @note Paho startConnectRetry requires shouldBeConnected == 1, which is true
 *       unless the user called MQTTAsync_disconnect().
 */
inline void uvbasic_client::on_connect_lost(char* _cause)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_lost | status={} disc_req={} wait_rest={} destroying={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire));

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        // User requested disconnect; don't start reconnect, just notify.
        // Defensively clean up state in case on_disconnect_success never
        // fires (e.g. TCP drop before DISCONNECT ACK arrives). Without
        // this reset the client would be permanently stuck in
        // "disconnecting" state with no automatic recovery path.
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
        disconnect_requested_.store(false, std::memory_order_release);
        if (connect_lost_cb_)
            connect_lost_cb_(weak_from_this(), _cause);
        return;
    }

    if (conn_opts_.raw().automaticReconnect != 0)
    {
        // If connect_status_ is already connecting, user called connect()
        // concurrently and a fresh connect is in flight.  Only set up
        // reconnect tracking when we are NOT in that state, otherwise
        // on_connected would misclassify the fresh connect as a
        // Paho auto-reconnect recovery and spuriously fire reconnected_cb_.
        auto st = connect_status_.value.load(std::memory_order_acquire);
        if (st != connect_status::connecting)
        {
            wait_conn_restored_.store(true, std::memory_order_release);
            // Paho's startConnectRetry handles the actual reconnection;
            // shouldBeConnected is still 1 since user didn't call MQTTAsync_disconnect.
            // Update status to reflect that a reconnect attempt is in progress.
            connect_status_.value.store(connect_status::connecting, std::memory_order_release);
            if (log_lvl_ <= log_level::trace)
                _log(log_level::trace, "uvbasic_client({}) status ->connecting | caller=on_connect_lost",
                    create_opts_.client_id());
        }

        // Start health-check timer for periodic reconnect-stalled notifications
        // and MQTTAsync_isConnected polling (Plans B + D).
        reconnect_start_time_ms_.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_release);
        // Delegate uv_timer_start to the event-loop thread — uv_timer_start
        // is not thread-safe and this callback runs on Paho's internal thread.
        health_check_timer_needs_start_.store(true, std::memory_order_release);
        {
            std::unique_lock<std::mutex> locker(mtx_);
            if (retry_connect_async_cancel_ &&
                !destroying_.load(std::memory_order_acquire))  // F-01 fix
                uv_async_send(retry_connect_async_cancel_.get());
        }
    }
    else
    {
        // No auto-reconnect — must update the state machine
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
        wait_conn_restored_.store(false, std::memory_order_release);
    }

    if (connect_lost_cb_)
        connect_lost_cb_(weak_from_this(), _cause);
}

/**
 * @brief Paho callback: the connection has been established (or re-established).
 *
 * Always fires AFTER on_connect_success/success5.  Paho passes a reason string:
 *   - "connect onSuccess called" for first-time connections.
 *   - "automatic reconnect" for Paho auto-reconnect.
 *
 * ## Behavior:
 *   - If disconnect_requested_ is true: NO-OP -- on_connect_success already
 *     handled the disconnect path.  This prevents a duplicate
 *     MQTTAsync_disconnect call.
 *   - If wait_conn_restored_ is true (reconnect recovery): clears the flag,
 *     stops the health-check timer, updates connect_status_, and calls
 *     reconnected_cb_ before connected_cb_.
 *   - Otherwise (first connect): just calls connected_cb_.
 *
 * ## Callback ordering (reconnect):
 *   connect_success_cb_ -> reconnected_cb_ -> connected_cb_
 *
 * ## Callback ordering (first connect):
 *   connect_success_cb_ -> connected_cb_
 *
 * @param _cause  Reason string from Paho.
 * @note Thread: Paho internal thread (from CONNACK handler).
 */
inline void uvbasic_client::on_connected(char* _cause)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connected | status={} disc_req={} wait_rest={} destroying={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire));

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        // Two paths reach here:
        //   Normal: on_connect_success already handled the disconnect
        //           (set connect_status_=disconnecting + called MQTTAsync_disconnect).
        //           No-op to avoid a duplicate MQTTAsync_disconnect call.
        //   Race:   on_connect_success read disconnect_requested_=false before
        //           the user thread set it, set connect_status_=connected, and
        //           called us.  disconnect() saw connecting and returned OK
        //           expecting the callback to handle it.  We must handle it here
        //           to prevent disconnect_requested_ from being stuck forever.
        if (connect_status_.value.load(std::memory_order_acquire) == connect_status::connected)
        {
            connect_status_.value.store(connect_status::disconnecting, std::memory_order_release);
            // Lock to safely read native_cli_ — this callback runs on Paho's
            // internal thread, while on_destroy() may concurrently write
            // native_cli_ = nullptr under mtx_.
            MQTTAsync hdl;
            MQTTAsync_disconnectOptions disconn_opts_copy;
            {
                std::unique_lock<std::mutex> locker(mtx_);
                hdl = native_cli_;
                // Snapshot disconn_opts_ under the lock to avoid a data race
                // with set_disconn_opts() on a user thread (F-04 fix).
                disconn_opts_copy = disconn_opts_.raw();
            }
            if (hdl)
            {
                int rc = MQTTAsync_disconnect(hdl, &disconn_opts_copy);
                if (rc != MQTTASYNC_SUCCESS)
                {
                    // Paho returned a synchronous error (e.g. MQTTASYNC_DISCONNECTED
                    // because m->c->connected==0).  No async callback will fire, so
                    // clean up here to prevent disconnect_requested_ from being stuck.
                    connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
                    disconnect_requested_.store(false, std::memory_order_release);
                }
            }
        }
        return;
    }

    if (wait_conn_restored_.load(std::memory_order_acquire)) {
        wait_conn_restored_.store(false, std::memory_order_release);

        // Stop the health-check timer — reconnect has succeeded.
        // Delegate uv_timer_stop to the event-loop thread — uv_timer_stop
        // is not thread-safe and this callback runs on Paho's internal thread.
        {
            std::unique_lock<std::mutex> locker(mtx_);
            if (retry_connect_async_cancel_ &&
                !destroying_.load(std::memory_order_acquire))  // F-01 fix
                uv_async_send(retry_connect_async_cancel_.get());
        }

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

/**
 * @brief Paho callback: the server sent a DISCONNECT packet (MQTT v5 only).
 *
 * ## Triggered ONLY by server-initiated DISCONNECT (MQTT v5).
 * NOT called for: user disconnect, TCP drop, or connect timeout.
 *
 * ## Behavior:
 *   - If disconnect_requested_ is true: only notifies disconnected_cb_.
 *     on_disconnect_success will handle state cleanup.
 *   - Otherwise (server-initiated): sets connect_status_ = disconnected,
 *     notifies disconnected_cb_.  Paho will subsequently call on_connect_lost
 *     (via nextOrClose) and may start auto-reconnect.
 *
 * @param _response  MQTT v5 properties from the DISCONNECT packet.
 * @param _reason    MQTT v5 reason code.
 * @note Thread: Paho internal thread (from packet receive handler).
 * @note Paho sets connected = 0 BEFORE calling nextOrClose, so subsequent
 *       on_connect_lost sees was_connected = 0.
 */
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

/**
 * @brief Generic success callback (MQTT v3).
 *
 * Forwards to success_cb_ with MQTTVERSION_DEFAULT.  success_cb_ is read
 * under mtx_ as shared_ptr for safe concurrent access.
 *
 * @note Thread: Paho internal thread.
 */
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

/**
 * @brief Generic failure callback (MQTT v3).
 * @note Thread: Paho internal thread.
 */
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

/**
 * @brief Generic success callback (MQTT v5).
 * @note Thread: Paho internal thread.
 */
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

/**
 * @brief Generic failure callback (MQTT v5).
 * @note Thread: Paho internal thread.
 */
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

/**
 * @brief Paho callback: MQTT CONNECT succeeded (MQTT v3).
 *
 * ## Disconnect-during-connect path (disconnect_requested_ == true):
 *   User called disconnect() while connecting.  The connection just succeeded,
 *   so we immediately call MQTTAsync_disconnect().  native_cli_ is read under
 *   mtx_ to avoid races with on_destroy().  The return value of
 *   MQTTAsync_disconnect() is checked: if Paho returns MQTTASYNC_DISCONNECTED
 *   synchronously (because connected==0), we defensively clear the state flags
 *   since no async callback will fire.
 *
 * ## Normal path:
 *   Sets connect_status_ = connected, resets the backoff to 1s, and calls
 *   connect_success_cb_.
 *
 * @param _response  Paho success data (server URI, MQTT version, session present).
 * @note Thread: Paho internal thread (from CONNACK handler).
 * @note Paho nulls m->connect.onSuccess and m->connect.onFailure after this call.
 */
inline void uvbasic_client::on_connect_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_success | status={} disc_req={} wait_rest={} destroying={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire));

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        // User called disconnect() while connecting; we've just connected — disconnect now.
        connect_status_.value.store(connect_status::disconnecting, std::memory_order_release);
        // Lock to safely read native_cli_ — this callback runs on Paho's internal thread,
        // while on_destroy() may concurrently write native_cli_ = nullptr under mtx_.
        MQTTAsync hdl;
        MQTTAsync_disconnectOptions disconn_opts_copy;
        {
            std::unique_lock<std::mutex> locker(mtx_);
            hdl = native_cli_;
            // Snapshot disconn_opts_ under the lock to avoid a data race
            // with set_disconn_opts() on a user thread (F-04 fix).
            disconn_opts_copy = disconn_opts_.raw();
        }
        if (hdl)
        {
            int rc = MQTTAsync_disconnect(hdl, &disconn_opts_copy);
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
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({}) status ->connected | caller=on_connect_success",
            create_opts_.client_id());

    // Reset backoff on successful connection
    retry_connect_backoff_ms_.store(1000, std::memory_order_release);

    if (connect_success_cb_)
        connect_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: MQTT CONNECT failed (MQTT v3).
 *
 * ## Disconnect-during-connect path (disconnect_requested_ == true):
 *   User called disconnect() while connecting, and the connect failed.
 *   Sets connect_status_ = disconnected and clears disconnect_requested_.
 *   Does NOT call connect_failure_cb_ (the user expects a disconnect result).
 *
 * ## Normal path with automaticReconnect:
 *   Sets wait_conn_restored_ = true and starts the health-check timer.
 *   Paho startConnectRetry handles the retry internally.
 *   connect_status_ stays at connecting (already set by __connect_mt()).
 *
 * ## Normal path without automaticReconnect:
 *   Sets connect_status_ = disconnected.  User must call connect() manually.
 *
 * @param _response  Paho failure data (error code, message).
 * @note Thread: Paho internal thread (from nextOrClose or CONNACK error path).
 * @note Paho calls this at most TWICE: once for the initial connect failure,
 *       and once for the first auto-reconnect failure via nextOrClose.
 *       After that, m->connect.onFailure is NULL and failures are silent.
 */
inline void uvbasic_client::on_connect_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_failure | status={} disc_req={} wait_rest={} destroying={} code={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire), _response ? _response->code : -1);

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
        // If connect_status_ is already connecting, user called connect()
        // concurrently and a fresh connect is in flight.  Only set up
        // reconnect tracking when we are NOT in that state, otherwise
        // on_connected would misclassify the fresh connect as a
        // Paho auto-reconnect recovery and spuriously fire reconnected_cb_.
        auto st = connect_status_.value.load(std::memory_order_acquire);
        if (st != connect_status::connecting)
        {
            wait_conn_restored_.store(true, std::memory_order_release);
        }

        // Start health-check timer for periodic reconnect-stalled notifications
        // and MQTTAsync_isConnected polling (Plans B + D).
        reconnect_start_time_ms_.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_release);
        // Delegate uv_timer_start to the event-loop thread — uv_timer_start
        // is not thread-safe and this callback runs on Paho's internal thread.
        health_check_timer_needs_start_.store(true, std::memory_order_release);
        {
            std::unique_lock<std::mutex> locker(mtx_);
            if (retry_connect_async_cancel_ &&
                !destroying_.load(std::memory_order_acquire))  // F-01 fix
                uv_async_send(retry_connect_async_cancel_.get());
        }
    }
    else
    {
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
        // F-N4 fix: defensively clear wait_conn_restored_ to prevent a
        // stale flag from causing a spurious reconnected_cb_ call if
        // on_connected fires later.  Symmetric with on_connect_lost's
        // no-auto-reconnect branch (line 1763).
        wait_conn_restored_.store(false, std::memory_order_release);
    }

    if (connect_failure_cb_)
        connect_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: MQTT CONNECT succeeded (MQTT v5).
 *
 * Identical logic to on_connect_success() but forwards MQTTVERSION_5
 * in the callback.
 *
 * @param _response  Paho v5 success data (includes properties, reason code).
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_connect_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_success5 | status={} disc_req={} wait_rest={} destroying={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire));

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        connect_status_.value.store(connect_status::disconnecting, std::memory_order_release);
        // Lock to safely read native_cli_ — this callback runs on Paho's internal thread,
        // while on_destroy() may concurrently write native_cli_ = nullptr under mtx_.
        MQTTAsync hdl;
        MQTTAsync_disconnectOptions disconn_opts_copy;
        {
            std::unique_lock<std::mutex> locker(mtx_);
            hdl = native_cli_;
            // Snapshot disconn_opts_ under the lock to avoid a data race
            // with set_disconn_opts() on a user thread (F-04 fix).
            disconn_opts_copy = disconn_opts_.raw();
        }
        if (hdl)
        {
            int rc = MQTTAsync_disconnect(hdl, &disconn_opts_copy);
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
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({}) status ->connected | caller=on_connect_success5",
            create_opts_.client_id());

    // Reset backoff on successful connection
    retry_connect_backoff_ms_.store(1000, std::memory_order_release);

    if (connect_success_cb_)
        connect_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: MQTT CONNECT failed (MQTT v5).
 *
 * Identical logic to on_connect_failure() but forwards MQTTVERSION_5
 * in the callback.
 *
 * @param _response  Paho v5 failure data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_connect_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_connect_failure5 | status={} disc_req={} wait_rest={} destroying={} reasonCode={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire), _response ? static_cast<int>(_response->reasonCode) : -1);

    if (disconnect_requested_.load(std::memory_order_acquire))
    {
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
        disconnect_requested_.store(false, std::memory_order_release);
        return;
    }

    if (conn_opts_.raw().automaticReconnect != 0)
    {
        // If connect_status_ is already connecting, user called connect()
        // concurrently and a fresh connect is in flight.  Only set up
        // reconnect tracking when we are NOT in that state, otherwise
        // on_connected would misclassify the fresh connect as a
        // Paho auto-reconnect recovery and spuriously fire reconnected_cb_.
        auto st = connect_status_.value.load(std::memory_order_acquire);
        if (st != connect_status::connecting)
        {
            wait_conn_restored_.store(true, std::memory_order_release);
        }

        // Start health-check timer for periodic reconnect-stalled notifications
        // and MQTTAsync_isConnected polling (Plans B + D).
        reconnect_start_time_ms_.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_release);
        // Delegate uv_timer_start to the event-loop thread — uv_timer_start
        // is not thread-safe and this callback runs on Paho's internal thread.
        health_check_timer_needs_start_.store(true, std::memory_order_release);
        {
            std::unique_lock<std::mutex> locker(mtx_);
            if (retry_connect_async_cancel_ &&
                !destroying_.load(std::memory_order_acquire))  // F-01 fix
                uv_async_send(retry_connect_async_cancel_.get());
        }
    }
    else
    {
        connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
        // F-N4 fix: defensively clear wait_conn_restored_ (see on_connect_failure).
        wait_conn_restored_.store(false, std::memory_order_release);
    }

    if (connect_failure_cb_)
        connect_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: user-initiated disconnect completed (MQTT v3).
 *
 * Sets connect_status_ = disconnected, clears disconnect_requested_,
 * and calls disconnect_success_cb_.
 *
 * @param _response  Paho success data.
 * @note Thread: Paho internal thread (from checkDisconnect).
 * @note This is the terminal state for a normal user disconnect.
 */
inline void uvbasic_client::on_disconnect_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_disconnect_success | status={} disc_req={} wait_rest={} destroying={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire));

    connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    disconnect_requested_.store(false, std::memory_order_release);

    if (disconnect_success_cb_)
        disconnect_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: user-initiated disconnect failed (MQTT v3).
 *
 * Paho currently NEVER calls onFailure for disconnect (checkDisconnect only
 * calls onSuccess/onSuccess5).  This handler exists as defensive code: if a
 * future Paho version adds the failure path, we clean up state rather than
 * leaving disconnect_requested_ stuck and connect_status_ at disconnecting.
 *
 * @param _response  Paho failure data.
 * @note Thread: Paho internal thread (currently never invoked).
 */
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

/**
 * @brief Paho callback: user-initiated disconnect completed (MQTT v5).
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_disconnect_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_disconnect_success5 | status={} disc_req={} wait_rest={} destroying={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire));

    connect_status_.value.store(connect_status::disconnected, std::memory_order_release);
    disconnect_requested_.store(false, std::memory_order_release);

    if (disconnect_success_cb_)
        disconnect_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: user-initiated disconnect failed (MQTT v5).
 * Defensive handler -- Paho never calls this in current versions.
 * @note Thread: Paho internal thread.
 */
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

/**
 * @brief Paho callback: subscribe succeeded (MQTT v3).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_subscribe_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_subscribe_success",
            create_opts_.client_id());
    
    if (subscribe_success_cb_)
        subscribe_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: subscribe failed (MQTT v3).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_subscribe_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_subscribe_failure",
            create_opts_.client_id());
    
    if (subscribe_failure_cb_)
        subscribe_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: subscribe succeeded (MQTT v5).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_subscribe_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_subscribe_success5",
            create_opts_.client_id());
    
    if (subscribe_success_cb_)
        subscribe_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: subscribe failed (MQTT v5).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_subscribe_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_subscribe_failure5",
            create_opts_.client_id());
    
    if (subscribe_failure_cb_)
        subscribe_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: unsubscribe succeeded (MQTT v3).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_unsubscribe_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_unsubscribe_success",
            create_opts_.client_id());
    
    if (unsubscribe_success_cb_)
        unsubscribe_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: unsubscribe failed (MQTT v3).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_unsubscribe_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_unsubscribe_failure",
            create_opts_.client_id());
    
    if (unsubscribe_failure_cb_)
        unsubscribe_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: unsubscribe succeeded (MQTT v5).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_unsubscribe_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_unsubscribe_success5",
            create_opts_.client_id());
    
    if (unsubscribe_success_cb_)
        unsubscribe_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: unsubscribe failed (MQTT v5).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_unsubscribe_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_unsubscribe_failure5",
            create_opts_.client_id());
    
    if (unsubscribe_failure_cb_)
        unsubscribe_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: publish succeeded (MQTT v3).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_publish_success(MQTTAsync_successData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_publish_success",
            create_opts_.client_id());
    
    if (publish_success_cb_)
        publish_success_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: publish failed (MQTT v3).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_publish_failure(MQTTAsync_failureData* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_publish_failure",
            create_opts_.client_id());
    
    if (publish_failure_cb_)
        publish_failure_cb_(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

/**
 * @brief Paho callback: publish succeeded (MQTT v5).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_publish_success5(MQTTAsync_successData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_publish_success5",
            create_opts_.client_id());
    
    if (publish_success_cb_)
        publish_success_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: publish failed (MQTT v5).
 *
 * Forwards to the corresponding user callback.
 *
 * @param _response  Paho operation result data.
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::on_publish_failure5(MQTTAsync_failureData5* _response)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_publish_failure5",
            create_opts_.client_id());
    
    if (publish_failure_cb_)
        publish_failure_cb_(weak_from_this(), MQTTVERSION_5, _response);
}

/**
 * @brief Paho callback: SSL certificate verification error.
 *
 * @param _str  Error description.
 * @param _len  Length of the error string.
 * @return 1 to continue (accept the certificate anyway), 0 to abort.
 * @note Thread: Paho internal thread (OpenSSL callback context).
 */
inline int uvbasic_client::on_ssl_error(const char* _str, size_t _len)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_ssl_error",
            create_opts_.client_id());
    
    if (ssl_error_cb_)
        return ssl_error_cb_(weak_from_this(), _str, _len);
    return 1;
}

/**
 * @brief Paho callback: SSL Pre-Shared Key (PSK) identity and key request.
 *
 * @param _hint              PSK hint from the server.
 * @param _identity          Output buffer for the PSK identity.
 * @param _max_identity_len  Maximum length of identity.
 * @param _psk               Output buffer for the PSK key.
 * @param _max_psk_len       Maximum length of PSK key.
 * @return The length of the PSK key written, or 0 on error.
 * @note Thread: Paho internal thread (OpenSSL callback context).
 */
inline unsigned int uvbasic_client::on_ssl_psk(const char* _hint, char* _identity, unsigned int _max_identity_len, unsigned char* _psk, unsigned int _max_psk_len)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_ssl_psk",
            create_opts_.client_id());
    
    if (ssl_psk_cb_)
        return ssl_psk_cb_(weak_from_this(), _hint, _identity, _max_identity_len, _psk, _max_psk_len);
    return 1;
}

/**
 * @brief libuv callback: destroy signal received (event-loop thread).
 *
 * Closes all libuv handles in sequence:
 *   1. Closes the destroy async handle itself (uv_close with callback).
 *   2. Under mtx_: closes retry_connect_async_req_ and retry_connect_async_cancel_
 *      FIRST to block any pending uv_async_send from reaching callbacks
 *      that would operate on timer handles (F-07 fix).
 *   3. Stops and closes retry_connect_timer_ and health_check_timer_.
 *
 * Each close callback decrements handle_counter_.  When it reaches 0,
 * on_destroy() is invoked.
 *
 * @param _handle  The destroy async handle.
 * @note Thread: libuv event-loop thread (via uv_async_send from destroy_request()).
 * @note Calls uv_close on retry_connect_async_req_ and cancel under mtx_ because
 *       their close callbacks access mtx_-protected members.
 */
inline void uvbasic_client::on_destroy_async_call(uv_async_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_destroy_async_call",
            create_opts_.client_id());

    // Set the atomic flag BEFORE any uv_close() so that concurrent
    // uv_async_send calls on other threads see the flag and bail out
    // instead of calling uv_async_send on a closing handle (F-01 fix).
    destroying_.store(true, std::memory_order_release);

    uv_close(reinterpret_cast<uv_handle_t*>(_handle), __on_destroy_async_close);

    // F-07 fix: close async handles FIRST (under mtx_) to block any pending
    // uv_async_send from reaching cancel/retry callbacks.  Only then close
    // timer handles — preventing a use-after-close where a pending cancel
    // callback would uv_timer_start() on an already-closing timer.
    std::unique_lock<std::mutex> locker(mtx_);
    if (retry_connect_async_req_)
    {
        uv_close(reinterpret_cast<uv_handle_t*>(retry_connect_async_req_.get()), __on_retry_connect_async_close);
    }
    
    if (retry_connect_async_cancel_)
    {
        uv_close(reinterpret_cast<uv_handle_t*>(retry_connect_async_cancel_.get()), __on_retry_connect_cancel_close);
    }
    locker.unlock();

    if (retry_connect_timer_) {
        uv_timer_stop(retry_connect_timer_.get());
        uv_close(reinterpret_cast<uv_handle_t*>(retry_connect_timer_.get()), __on_retry_connect_timer_close);
    }

    if (health_check_timer_) {
        uv_timer_stop(health_check_timer_.get());
        uv_close(reinterpret_cast<uv_handle_t*>(health_check_timer_.get()), __on_health_check_timer_close);
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

/**
 * @brief libuv callback: destroy async handle has been closed.
 *
 * Resets destroy_async_req_ under mtx_, retains self_ to keep the object alive,
 * and decrements handle_counter_.
 *
 * @param _handle  The closed uv handle.
 * @note Thread: libuv event-loop thread.
 */
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

/**
 * @brief libuv callback: retry-connect signal received.
 *
 * Resets retry_connect_backoff_ms_ to 1s and starts retry_connect_timer_
 * as a one-shot timer.  If retry_connect_timer_ is null (destroy in progress),
 * clears auto_reconn_hdl_running_.
 *
 * @param _handle  The retry-connect async handle.
 * @note Thread: libuv event-loop thread (via uv_async_send from connect()).
 */
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

/**
 * @brief libuv callback: retry-connect async handle closed.
 * @note Thread: libuv event-loop thread.
 */
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

/**
 * @brief libuv callback: cancel signal received (cross-thread timer stop).
 *
 * Stops both retry_connect_timer_ and health_check_timer_ on the event-loop
 * thread, then clears auto_reconn_hdl_running_.
 *
 * This is the safe cross-thread mechanism for stopping timers: disconnect()
 * (callable from any thread) sends uv_async_send to this handle, and this
 * callback runs uv_timer_stop on the event-loop thread where it is safe.
 *
 * @param _handle  The cancel async handle.
 * @note Thread: libuv event-loop thread.
 * @see disconnect() Phase 2, libuv design.rst thread-safety note.
 */
inline void uvbasic_client::on_retry_connect_cancel_call(uv_async_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_retry_connect_cancel_call",
            create_opts_.client_id());

    // Stop both timers on the event-loop thread — uv_timer_stop is not
    // thread-safe (per libuv design.rst), and this callback runs on the
    // event-loop thread via uv_async_send from disconnect() or Paho callbacks.
    if (retry_connect_timer_)
        uv_timer_stop(retry_connect_timer_.get());
    if (health_check_timer_)
        uv_timer_stop(health_check_timer_.get());
    __set_auto_reconn_hdl_running_mt(false);

    // Start the health-check timer if delegated from a Paho callback.
    // uv_timer_start is NOT thread-safe; this callback runs on the event-loop
    // thread, so it is safe to start the timer here.
    // F-06 fix: also check disconnect_requested_ — if the user called
    // disconnect() and its uv_async_send merged with a Paho callback's
    // uv_async_send that set health_check_timer_needs_start_=true, we
    // must not start the health-check timer (disconnect() stopped it).
    if (health_check_timer_needs_start_.load(std::memory_order_acquire) &&
        !disconnect_requested_.load(std::memory_order_acquire))
    {
        health_check_timer_needs_start_.store(false, std::memory_order_release);
        if (health_check_timer_)
            uv_timer_start(health_check_timer_.get(), __on_health_check_timer_call,
                           health_check_interval_sec_.load(std::memory_order_acquire) * 1000, 0);
    }
}

/**
 * @brief libuv callback: cancel async handle closed.
 * @note Thread: libuv event-loop thread.
 */
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

/**
 * @brief libuv callback: wrapper-level retry timer fired.
 *
 * ## Pre-checks (all on event-loop thread):
 *   1. disconnect_requested_? -- stop timer, clear flag, return.
 *   2. destroy_async_req_ exists? native_cli_ exists? connected already?
 *      running flag set?  All checked under mtx_ with a scope_cleanup that
 *      stops the timer on early exit.
 *
 * ## Core logic:
 *   1. Calls __connect_mt() to attempt a fresh MQTTAsync_connect().
 *   2. On success: the cleanup scope stops the timer.  Paho callbacks handle
 *      the result.
 *   3. On failure: cancels the cleanup, applies exponential backoff
 *      (1s -> 2s -> 4s -> ... -> 16s cap), and restarts the timer as one-shot.
 *
 * @param _handle  The retry timer handle.
 * @note Thread: libuv event-loop thread.
 * @note Uses one-shot timer (repeat=0) -- restarted manually with new interval.
 */
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

    // CR-1 fix: unlock mtx_ before calling MQTTAsync_isConnected().
    // The cleanup lambda will re-acquire mtx_ if needed when we return
    // early (locker.owns_lock() is checked inside the cleanup).
    locker.unlock();

    if (__is_connected_safe())
        return;

    if (!__auto_reconn_hdl_running_mt())
        return;

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

/**
 * @brief libuv callback: retry timer handle closed.
 * @note Thread: libuv event-loop thread.
 */
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

/**
 * @brief libuv callback: health-check timer fired (Plans B + D).
 *
 * Active only while wait_conn_restored_ is true (Paho silently retrying).
 *
 * ## Each tick:
 *   1. Bail if disconnect_requested_ is true.
 *   2. Bail if wait_conn_restored_ has been cleared (on_connected fired first).
 *   3. **Plan D**: Under mtx_, poll MQTTAsync_isConnected(native_cli_).
 *      If Paho reports connected but the wrapper has not noticed -- auto-fix:
 *      clear wait_conn_restored_, update connect_status_, stop timer,
 *      fire reconnected_cb_ + connected_cb_.
 *   4. **Plan B**: If reconnect_stalled_cb_ is set, call it with elapsed seconds
 *      since reconnect_start_time_ms_.
 *   5. Restart the timer for the next tick.
 *
 * @param _handle  The health-check timer handle.
 * @note Thread: libuv event-loop thread.
 * @see reconnect_stalled_callback, set_health_check_interval()
 */
inline void uvbasic_client::on_health_check_timer_call(uv_timer_t* _handle)
{
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({})::on_health_check_timer_call | status={} disc_req={} wait_rest={} destroying={}",
            create_opts_.client_id(), static_cast<int>(connect_status_.value.load(std::memory_order_acquire)), disconnect_requested_.load(std::memory_order_acquire), wait_conn_restored_.load(std::memory_order_acquire), destroying_.load(std::memory_order_acquire));
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
        // CR-1 fix: read native_cli_ under mtx_, then release the lock
        // before calling MQTTAsync_isConnected().  Prevents ABBA deadlock
        // with Paho callback threads (see __is_connected_safe() doc).
        MQTTAsync hdl = nullptr;
        {
            std::unique_lock<std::mutex> locker(mtx_);
            hdl = native_cli_;
        }
        if (hdl && MQTTAsync_isConnected(hdl))
        {
            // Re-acquire mtx_ for state updates that must be atomic.
            std::unique_lock<std::mutex> locker(mtx_);
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
        auto start_ms = std::chrono::milliseconds(
            reconnect_start_time_ms_.load(std::memory_order_acquire));
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now_ms - start_ms).count();
        reconnect_stalled_cb_(weak_from_this(), elapsed);
    }

    // Restart for next tick
    uv_timer_start(_handle, __on_health_check_timer_call,
                   health_check_interval_sec_.load(std::memory_order_acquire) * 1000, 0);
}

/**
 * @brief libuv callback: health-check timer handle closed.
 * @note Thread: libuv event-loop thread.
 */
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

/**
 * @brief Final destruction callback -- invoked when all libuv handles are closed.
 *
 * Triggered by handle_counter_ reaching 0.  Performs:
 *   1. Extracts native_cli_ under mtx_ and sets it to nullptr (prevents any
 *      in-flight Paho callbacks from accessing the handle).
 *   2. Calls MQTTAsync_destroy(&hdl) which internally:
 *      - Sends a DISCONNECT packet (if connected).
 *      - Closes the socket.
 *      - Frees all Paho-allocated resources.
 *   3. self_ is reset on scope exit (via MEGOPP_UTIL__ON_SCOPE_CLEANUP),
 *      releasing the final shared_ptr reference.
 *
 * @note Thread: libuv event-loop thread (triggered by last close callback).
 * @note MQTTAsync_destroy() is called WITHOUT mtx_ held because it may
 *       invoke callbacks that also acquire mtx_ leading to potential deadlock.
 * @note No prior MQTTAsync_disconnect() is needed; destroy handles it internally.
 */
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

/**
 * @brief Internal: performs the actual MQTTAsync_connect() call.
 *
 * ## Pre-condition checks:
 *   1. native_cli_ is non-null (under mtx_).
 *   2. disconnect_requested_ is false (acquire).
 *   3. connect_status_ == disconnected (acquire).
 *
 * ## Execution:
 *   1. Sets connect_status_ = connecting (release).
 *   2. Calls MQTTAsync_connect(hdl, &conn_opts_.raw()).
 *   3. On synchronous failure: rolls back to disconnected and, if
 *      disconnect_requested_ is now true (disconnect() called concurrently),
 *      clears the flag to prevent permanent deadlock.
 *
 * @return mgpp::err -- OK if the connect was enqueued with Paho.
 * @retval MGEC__ALREADY   Already connected or connecting, or native_cli_ is null.
 * @retval MGEC__INPROGRESS disconnect_requested_ is true.
 * @retval MGEC__ERR       MQTTAsync_connect() returned a synchronous error.
 *
 * @note Thread: caller's thread (connect() or retry timer callback).
 * @note The _mt suffix is historical; no mutex is held during the Paho call.
 */
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
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({}) status ->connecting | caller=__connect_mt",
            create_opts_.client_id());

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

/**
 * @brief Internal: performs the actual MQTTAsync_disconnect() call.
 *
 * ## Pre-condition checks:
 *   1. native_cli_ is non-null (under mtx_).
 *   2. connect_status_ == connected (acquire).
 *
 * ## Execution:
 *   1. Sets connect_status_ = disconnecting (release).
 *   2. Calls MQTTAsync_disconnect(hdl, &disconn_opts_.raw()).
 *   3. On synchronous failure (e.g. Paho returns MQTTASYNC_DISCONNECTED
 *      because m->c->connected == 0): rolls back to connected and clears
 *      disconnect_requested_ defensively.
 *
 * @return mgpp::err -- OK if the disconnect was enqueued with Paho.
 * @retval MGEC__ALREADY  Already disconnected, or native_cli_ is null.
 * @retval MGEC__ERR      MQTTAsync_disconnect() returned a synchronous error.
 *
 * @note Thread: caller's thread (disconnect()).
 * @note Paho MQTTAsync_disconnect() sets shouldBeConnected = 0, which stops
 *       Paho internal auto-reconnect.
 */
inline mgpp::err uvbasic_client::__disconnect_mt()
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto hdl = native_cli_;
    if (hdl == nullptr)
        return mgpp::err{ MGEC__ALREADY, "already disconnected" };
    locker.unlock();

    if (connect_status_.value.load(std::memory_order_acquire) != connect_status::connected)
    {
        // Bug fix: disconnect() set disconnect_requested_ = true before calling
        // __disconnect_mt().  If connect_status_ changed between the two checks
        // (TOCTOU), clear the flag so connect() is not permanently blocked.
        disconnect_requested_.store(false, std::memory_order_release);
        return mgpp::err{ MGEC__ALREADY, "already disconnected" };
    }
    connect_status_.value.store(connect_status::disconnecting, std::memory_order_release);
    if (log_lvl_ <= log_level::trace)
        _log(log_level::trace, "uvbasic_client({}) status ->disconnecting | caller=__disconnect_mt",
            create_opts_.client_id());

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
/**
 * @brief Static factory: creates, configures, and initialises a uvbasic_client.
 *
 * This is the only way to construct a uvbasic_client.  The constructor is private.
 *
 * ## Steps:
 *   1. Creates native create options from user options.
 *   2. Constructs the client (private constructor).
 *   3. Calls set_conn_opts() -- copies user connect options to native struct.
 *   4. Calls set_disconn_opts() -- copies user disconnect options to native struct.
 *   5. Calls init() -- creates the Paho handle and all libuv handles.
 *
 * @param _opts          User-level create options (client ID, persistence).
 * @param _conn_opts     User-level connect options (server URL, keep-alive, etc.).
 * @param _disconn_opts  User-level disconnect options (timeout, reason code).
 * @param _loop          The libuv event loop to attach handles to.
 * @return outcome::checked -- either a shared_ptr to the new client, or an error.
 *
 * @note The client is returned in the disconnected state; call connect() to begin.
 */
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

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline int uvbasic_client::__on_message_arrived(void* _context, char* _topic_name, int _topic_len, MQTTAsync_message* _message)
{
    return reinterpret_cast<uvbasic_client*>(_context)->on_message_arrived(_topic_name, _topic_len, _message);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_delivery_complete(void* _context, MQTTAsync_token _token)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_delivery_complete(_token);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_connect_lost(void *_context, char *_cause)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_lost(_cause);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_connected(void* _context, char* _cause)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connected(_cause);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_disconnected(void* _context, MQTTProperties* _response, enum MQTTReasonCodes _reason)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnected(_response, _reason);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_success(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_failure(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_success5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_failure5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_connect_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_success(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_connect_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_failure(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_connect_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_success5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_connect_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_connect_failure5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_disconnect_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnect_success(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_disconnect_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnect_failure(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_disconnect_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnect_success5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_disconnect_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_disconnect_failure5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_subscribe_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_subscribe_success(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_subscribe_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_subscribe_failure(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_subscribe_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_subscribe_success5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_subscribe_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_subscribe_failure5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_unsubscribe_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_unsubscribe_success(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_unsubscribe_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_unsubscribe_failure(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_unsubscribe_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_unsubscribe_success5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_unsubscribe_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_unsubscribe_failure5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_publish_success(void* _context, MQTTAsync_successData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_publish_success(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_publish_failure(void* _context, MQTTAsync_failureData* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_publish_failure(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_publish_success5(void* _context, MQTTAsync_successData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_publish_success5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline void uvbasic_client::__on_publish_failure5(void* _context, MQTTAsync_failureData5* _response)
{
    reinterpret_cast<uvbasic_client*>(_context)->on_publish_failure5(_response);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline int uvbasic_client::__on_ssl_error(const char* _str, size_t _len, void* _context)
{
    return reinterpret_cast<uvbasic_client*>(_context)->on_ssl_error(_str, _len);
}

/**
 * @brief Static C-linkage thunk: forwards the Paho callback to the C++ member.
 *
 * Paho C API expects plain C function pointers.  The user data pointer
 * (_context) is the `this` pointer of the uvbasic_client instance, set via
 * options.context.  This thunk reinterpret_casts and dispatches to the
 * corresponding on_*() method.
 *
 * @note Thread: Paho internal thread.
 */
inline unsigned int uvbasic_client::__on_ssl_psk(const char* _hint, char* _identity, unsigned int _max_identity_len, unsigned char* _psk, unsigned int _max_psk_len, void* _context)
{
    return reinterpret_cast<uvbasic_client*>(_context)->on_ssl_psk(_hint, _identity, _max_identity_len, _psk, _max_psk_len);
}
    

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_destroy_async_call(uv_async_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_destroy_async_call(_handle);
}

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_destroy_async_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_destroy_async_close(_handle);
}

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_retry_connect_async_call(uv_async_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_retry_connect_async_call(_handle);
}

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_retry_connect_async_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_retry_connect_async_close(_handle);
}


/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_retry_connect_cancel_call (uv_async_t * _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_retry_connect_cancel_call(_handle);
}

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_retry_connect_cancel_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_retry_connect_cancel_close(_handle);
}

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_retry_connect_timer_call(uv_timer_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_retry_connect_timer_call(_handle);
}

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_retry_connect_timer_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_retry_connect_timer_close(_handle);
}

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_health_check_timer_call(uv_timer_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_health_check_timer_call(_handle);
}

/**
 * @brief Static C-linkage thunk: forwards the libuv callback to the C++ member.
 *
 * libuv expects plain C function pointers for handle callbacks.  The user data
 * (set via uv_handle_set_data) is the `this` pointer.  This thunk retrieves it
 * and dispatches to the corresponding on_*() method.
 *
 * @note Thread: libuv event-loop thread.
 */
inline void uvbasic_client::__on_health_check_timer_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_health_check_timer_close(_handle);
}

}
}
}

#endif // !MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED
