
#ifndef MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED
#define MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED

#include "option.hpp"

#include <uv.h>

#include <megopp/err/err.h>
#include <megopp/util/scope_cleanup.h>
#include <megopp/util/simple_counter.h>

#include <mutex>
#include <functional>
#include <variant>

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
    typedef bool message_arrived_cb_t(const char*, int, MQTTAsync_message*);
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

    using message_arrived_callback   = std::function<message_arrived_cb_t>;
    using delivery_complete_callback = std::function<delivery_complete_cb_t>;

    using connect_lost_callback = std::function<connect_lost_cb_t>;
    using connected_callback    = std::function<connected_cb_t>;
    using disconnected_callback = std::function<disconnected_cb_t>;

    using update_connect_options_callback = std::function<update_connect_options_cb_t>;

    using success_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const success_data_t&)>;
    using failure_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const failure_data_t&)>;

    using connect_success_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const success_data_t&)>;
    using connect_failure_callback = std::function<void(const std::weak_ptr<uvbasic_client>&, int _mqtt_version, const failure_data_t&)>;

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

    void set_message_arrived_callback(const message_arrived_callback& _cb);
    void set_delivery_complete_callback(const delivery_complete_callback& _cb);

    void set_connect_lost_callback(const connect_lost_callback& _cb);
    void set_connected_callback(const connected_callback& _cb);
    void set_disconnected_callback(const disconnected_callback& _cb);

    // void set_update_connect_options_callback(const update_connect_options_callback& _cb);

    void set_success_callback(const success_callback& _cb);
    void set_failure_callback(const failure_callback& _cb);

    void set_connect_success_callback(const connect_success_callback& _cb);
    void set_connect_failure_callback(const connect_failure_callback& _cb);

    void set_subscribe_success_callback(const subscribe_success_callback& _cb);
    void set_subscribe_failure_callback(const subscribe_failure_callback& _cb);

    void set_unsubscribe_success_callback(const unsubscribe_success_callback& _cb);
    void set_unsubscribe_failure_callback(const unsubscribe_failure_callback& _cb);

    void set_publish_success_callback(const publish_success_callback& _cb);
    void set_publish_failure_callback(const publish_failure_callback& _cb);

    void set_ssl_error_callback(const ssl_error_callback& _cb);
    void set_ssl_psk_callback(const ssl_psk_callback& _cb);

    inline constexpr const create_native_options& create_opts() const noexcept { return create_opts_; }
    inline constexpr const connect_native_options& connect_opts() const noexcept { return conn_opts_; }
    inline constexpr const disconnect_native_options& disconnect_opts() const noexcept { return disconn_opts_; }
    
protected:
    int on_message_arrived(char* _topic_name, int _topic_len, MQTTAsync_message* _message);
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

    void on_async_destroy_call (uv_async_t * _handle);
    void on_async_destroy_close(uv_handle_t* _handle);

    void on_destroy();
    inline MQTTAsync native_st() const noexcept { return native_cli_; }
    inline MQTTAsync native_mt() const
    {
        std::lock_guard<std::mutex> locker(mtx_);
        return native_cli_;
    }
public:

    static outcome::checked<std::shared_ptr<uvbasic_client>, mgpp::err> 
        create(
            const create_native_options& _opts, 
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

    static void __on_async_destroy_call (uv_async_t * _handle);
    static void __on_async_destroy_close(uv_handle_t* _handle);

protected:

    mutable std::mutex mtx_;

    std::shared_ptr<void> self_;
    MQTTAsync native_cli_;
    create_native_options create_opts_;
    connect_native_options conn_opts_;
    disconnect_native_options disconn_opts_;

    std::shared_ptr<message_arrived_callback> message_arrived_cb_;
    std::shared_ptr<delivery_complete_callback> delivery_complete_cb_;

    std::shared_ptr<connect_lost_callback> connect_lost_cb_;
    std::shared_ptr<connected_callback> connected_cb_;
    std::shared_ptr<disconnected_callback> disconnected_cb_;

    std::shared_ptr<update_connect_options_callback> update_connect_options_cb_;
    
    std::shared_ptr<success_callback> success_cb_;
    std::shared_ptr<failure_callback> failure_cb_;
    
    std::shared_ptr<connect_success_callback> connect_success_cb_;
    std::shared_ptr<connect_failure_callback> connect_failure_cb_;
    
    std::shared_ptr<connect_success_callback> disconnect_success_cb_;
    std::shared_ptr<connect_failure_callback> disconnect_failure_cb_;

    std::shared_ptr<subscribe_success_callback> subscribe_success_cb_;
    std::shared_ptr<subscribe_failure_callback> subscribe_failure_cb_;

    std::shared_ptr<unsubscribe_success_callback> unsubscribe_success_cb_;
    std::shared_ptr<unsubscribe_failure_callback> unsubscribe_failure_cb_;

    std::shared_ptr<publish_success_callback> publish_success_cb_;
    std::shared_ptr<publish_failure_callback> publish_failure_cb_;

    std::shared_ptr<ssl_error_callback> ssl_error_cb_;
    std::shared_ptr<ssl_psk_callback> ssl_psk_cb_;

    mgpp::util::ref_counter<> handle_counter_;
    std::unique_ptr<uv_async_t> async_destroy_;
};


uvbasic_client::uvbasic_client(const create_native_options& _opts)
    : create_opts_(_opts)
    , conn_opts_(_opts.raw().MQTTVersion)
    , disconn_opts_(_opts.raw().MQTTVersion)
{
    sizeof(*this);
    
    handle_counter_.set_callback([this](auto&) 
    {
        on_destroy();
    });

    conn_opts_.raw().onSuccess  = __on_connect_success;
    conn_opts_.raw().onFailure  = __on_connect_failure;
    conn_opts_.raw().onSuccess5 = __on_connect_success5;
    conn_opts_.raw().onFailure5 = __on_connect_failure5;
    
    disconn_opts_.raw().onSuccess  = __on_disconnect_success;
    disconn_opts_.raw().onFailure  = __on_disconnect_failure;
    disconn_opts_.raw().onSuccess5 = __on_disconnect_success5;
    disconn_opts_.raw().onFailure5 = __on_disconnect_failure5;

    conn_opts_.raw().context = this;
    disconn_opts_.raw().context = this;
}

uvbasic_client::~uvbasic_client()
{
}

mgpp::err uvbasic_client::init(uv_loop_t* _loop)
{
    if (!_loop)
        return mgpp::err{ MGEC__INVAL, "invalid loop" };

    std::unique_lock<std::mutex> locker(mtx_);
    if (async_destroy_)
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

    auto async_destroy = std::make_unique<uv_async_t>();
    uv_async_init(_loop, async_destroy.get(), __on_async_destroy_call);
    uv_handle_set_data(reinterpret_cast<uv_handle_t*>(async_destroy.get()), this);

    handle_counter_.set_count(1);

    locker.lock();
    handle_cleanup.cancel();
    native_cli_ = handle;
    async_destroy_ = std::move(async_destroy);
    self_ = shared_from_this();
    return {};
}

inline void uvbasic_client::destroy_request()
{
    std::unique_lock<std::mutex> locker(mtx_);
    if (!async_destroy_)
        return;
    uv_async_send(async_destroy_.get());
}

inline void uvbasic_client::set_message_arrived_callback(const message_arrived_callback& _cb) 
{
    std::shared_ptr<message_arrived_callback> cb;
    if (_cb) {
        cb = std::make_shared<message_arrived_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    message_arrived_cb_ = cb;
}

inline void uvbasic_client::set_delivery_complete_callback(const delivery_complete_callback& _cb)
{
    std::shared_ptr<delivery_complete_callback> cb;
    if (_cb) {
        cb = std::make_shared<delivery_complete_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    delivery_complete_cb_ = cb;
}

inline void uvbasic_client::set_connect_lost_callback(const connect_lost_callback& _cb)
{
    std::shared_ptr<connect_lost_callback> cb;
    if (_cb) {
        cb = std::make_shared<connect_lost_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    connect_lost_cb_ = cb;
}

inline void uvbasic_client::set_connected_callback(const connected_callback& _cb)
{
    std::shared_ptr<connected_callback> cb;
    if (_cb) {
        cb = std::make_shared<connected_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    connected_cb_ = cb;
}

inline void uvbasic_client::set_disconnected_callback(const disconnected_callback& _cb)
{
    std::shared_ptr<disconnected_callback> cb;
    if (_cb) {
        cb = std::make_shared<disconnected_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    disconnected_cb_ = cb;
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
    std::shared_ptr<connect_success_callback> cb;
    if (_cb) {
        cb = std::make_shared<connect_success_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    connect_success_cb_ = cb;
}

inline void uvbasic_client::set_connect_failure_callback(const connect_failure_callback& _cb)
{
    std::shared_ptr<connect_failure_callback> cb;
    if (_cb) {
        cb = std::make_shared<connect_failure_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    connect_failure_cb_ = cb;
}

inline void uvbasic_client::set_subscribe_success_callback(const subscribe_success_callback& _cb)
{
    std::shared_ptr<subscribe_success_callback> cb;
    if (_cb) {
        cb = std::make_shared<subscribe_success_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    subscribe_success_cb_ = cb;
}

inline void uvbasic_client::set_subscribe_failure_callback(const subscribe_failure_callback& _cb)
{
    std::shared_ptr<subscribe_failure_callback> cb;
    if (_cb) {
        cb = std::make_shared<subscribe_failure_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    subscribe_failure_cb_ = cb;
}

inline void uvbasic_client::set_unsubscribe_success_callback(const unsubscribe_success_callback& _cb)
{
    std::shared_ptr<unsubscribe_success_callback> cb;
    if (_cb) {
        cb = std::make_shared<unsubscribe_success_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    unsubscribe_success_cb_ = cb;
}

inline void uvbasic_client::set_unsubscribe_failure_callback(const unsubscribe_failure_callback& _cb)
{
    std::shared_ptr<unsubscribe_failure_callback> cb;
    if (_cb) {
        cb = std::make_shared<unsubscribe_failure_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    unsubscribe_failure_cb_ = cb;
}

inline void uvbasic_client::set_publish_success_callback(const publish_success_callback& _cb)
{
    std::shared_ptr<publish_success_callback> cb;
    if (_cb) {
        cb = std::make_shared<publish_success_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    publish_success_cb_ = cb;
}

inline void uvbasic_client::set_publish_failure_callback(const publish_failure_callback& _cb)
{
    std::shared_ptr<publish_failure_callback> cb;
    if (_cb) {
        cb = std::make_shared<publish_failure_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    publish_failure_cb_ = cb;
}

inline void uvbasic_client::set_ssl_error_callback(const ssl_error_callback& _cb)
{
    std::shared_ptr<ssl_error_callback> cb;
    if (_cb) {
        cb = std::make_shared<ssl_error_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    ssl_error_cb_ = cb;
}

inline void uvbasic_client::set_ssl_psk_callback(const ssl_psk_callback& _cb)
{
    std::shared_ptr<ssl_psk_callback> cb;
    if (_cb) {
        cb = std::make_shared<ssl_psk_callback>(_cb);
    }
    std::unique_lock<std::mutex> locker(mtx_);
    ssl_psk_cb_ = cb;
}

inline int uvbasic_client::on_message_arrived(char* _topic_name, int _topic_len, MQTTAsync_message* _message)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = message_arrived_cb_;
    if (!cb) {
        return 1;
    }
    locker.unlock();

    auto cleanup = megopp::util::scope_cleanup__create([&] {
        MQTTAsync_freeMessage(&_message);
        MQTTAsync_free(_topic_name);
    });

    auto result = (*cb)(_topic_name, _topic_len, _message);
    if (!result) {
        cleanup.cancel();
        return 0;
    }
    return 1;
}

inline void uvbasic_client::on_delivery_complete(MQTTAsync_token _token)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = delivery_complete_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(_token);
}

inline void uvbasic_client::on_connect_lost(char* _cause)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = connect_lost_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(_cause);
}

inline void uvbasic_client::on_connected(char* _cause)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = connected_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(_cause);
}

inline void uvbasic_client::on_disconnected(MQTTProperties* _response, enum MQTTReasonCodes _reason)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = disconnected_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(_response, _reason);
}

inline void uvbasic_client::on_success(MQTTAsync_successData* _response)
{
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
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = connect_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_connect_failure(MQTTAsync_failureData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = connect_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_connect_success5(MQTTAsync_successData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = connect_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_connect_failure5(MQTTAsync_failureData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = connect_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_disconnect_success(MQTTAsync_successData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = disconnect_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_disconnect_failure(MQTTAsync_failureData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = disconnect_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_disconnect_success5(MQTTAsync_successData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = disconnect_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_disconnect_failure5(MQTTAsync_failureData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = disconnect_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_subscribe_success(MQTTAsync_successData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = subscribe_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_subscribe_failure(MQTTAsync_failureData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = subscribe_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_subscribe_success5(MQTTAsync_successData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = subscribe_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_subscribe_failure5(MQTTAsync_failureData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = subscribe_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_unsubscribe_success(MQTTAsync_successData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = unsubscribe_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_unsubscribe_failure(MQTTAsync_failureData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = unsubscribe_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_unsubscribe_success5(MQTTAsync_successData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = unsubscribe_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_unsubscribe_failure5(MQTTAsync_failureData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = unsubscribe_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_publish_success(MQTTAsync_successData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = publish_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_publish_failure(MQTTAsync_failureData* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = publish_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_DEFAULT, _response);
}

inline void uvbasic_client::on_publish_success5(MQTTAsync_successData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = publish_success_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline void uvbasic_client::on_publish_failure5(MQTTAsync_failureData5* _response)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = publish_failure_cb_;
    if (!cb) {
        return;
    }
    locker.unlock();

    (*cb)(weak_from_this(), MQTTVERSION_5, _response);
}

inline int uvbasic_client::on_ssl_error(const char* _str, size_t _len)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = ssl_error_cb_;
    if (!cb) {
        return 1;
    }
    locker.unlock();

    return (*cb)(weak_from_this(), _str, _len);
}

inline unsigned int uvbasic_client::on_ssl_psk(const char* _hint, char* _identity, unsigned int _max_identity_len, unsigned char* _psk, unsigned int _max_psk_len)
{
    std::unique_lock<std::mutex> locker(mtx_);
    auto cb = ssl_psk_cb_;
    if (!cb) {
        return 1;
    }
    locker.unlock();

    return (*cb)(weak_from_this(), _hint, _identity, _max_identity_len, _psk, _max_psk_len);
}

void uvbasic_client::on_async_destroy_call(uv_async_t* _handle)
{
    uv_close(reinterpret_cast<uv_handle_t*>(_handle), __on_async_destroy_close);

    std::unique_lock<std::mutex> locker(mtx_);
    if (MQTTAsync_isConnected(native_cli_)) {
        //MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
        //opts.context = this;
        //opts.onSuccess;
        //opts.onFailure;
        //opts.onSuccess5;
        //opts.onFailure5;
        //opts.timeout = 1000;
        //MQTTAsync_disconnect(native_cli_, &opts);
    }
}

void uvbasic_client::on_async_destroy_close(uv_handle_t* _handle)
{
    auto self = self_;
    --handle_counter_;
}

void uvbasic_client::on_destroy()
{
    auto self = self_;
    MEGOPP_UTIL__ON_SCOPE_CLEANUP([this] {
        self_.reset();
    });

    std::unique_lock<std::mutex> locker(mtx_);
    if (native_cli_) {
        MQTTAsync_destroy(&native_cli_);
    }
    locker.unlock();
    
}

inline outcome::checked<std::shared_ptr<uvbasic_client>, mgpp::err>
    uvbasic_client::create(
        const create_native_options& _opts,
        const connect_options& _conn_opts,
        const disconnect_options& _disconn_opts, 
        uv_loop_t* _loop)
{
    std::shared_ptr<uvbasic_client> cli(new uvbasic_client(_opts));
    auto e = cli->init(_loop);
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
    

inline void uvbasic_client::__on_async_destroy_call(uv_async_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(reinterpret_cast<uv_handle_t*>(_handle)));
    p->on_async_destroy_call(_handle);
}

inline void uvbasic_client::__on_async_destroy_close(uv_handle_t* _handle)
{
    auto p = reinterpret_cast<uvbasic_client*>(uv_handle_get_data(_handle));
    p->on_async_destroy_close(_handle);
}

}
}
}

#endif // !MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED
