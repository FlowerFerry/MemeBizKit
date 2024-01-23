
#ifndef MMBKPP_WRAP_WSPP_ROUTABLE_SERVER_HPP_INCLUDED
#define MMBKPP_WRAP_WSPP_ROUTABLE_SERVER_HPP_INCLUDED

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <regex>
#include <mutex>
#include <tuple>

#include <asio/steady_timer.hpp>
#include <websocketpp/server.hpp>

#include <memepp/convert/std/string.hpp>
#include <megopp/memory/hashable_weak_ptr.hpp>
#include <megopp/err/err.hpp>
#include <mego/err/ec_impl.h>

#ifndef MMBKPP_OPT__IPV6_ENABLED
#define MMBKPP_OPT__IPV6_ENABLED 1
#endif

namespace mmbkpp {
namespace wrap {
namespace wspp {

template <typename Config>
class routable_server 
{

public:

    routable_server();
    ~routable_server() = default;

    void set_port(uint16_t _port);

    mgpp::err init_asio(websocketpp::lib::asio::io_service* _io_service);
    mgpp::err start();
    mgpp::err run();
    mgpp::err stop_request();

    inline websocketpp::server<Config>& source() {
        return server_;
    }

    inline const websocketpp::server<Config>& source() const noexcept {
        return server_;
    }

    routable_server& http_post(std::string const& _pattern, websocketpp::http_handler const& _handler);
    routable_server& http_get (std::string const& _pattern, websocketpp::http_handler const& _handler);

private:
    struct ws_callbacks 
    {
        websocketpp::ping_handler         ping_handler;
        websocketpp::pong_handler         pong_handler;
        websocketpp::pong_timeout_handler pong_timeout_handler;
        websocketpp::open_handler         open_handler;
        websocketpp::close_handler        close_handler;
        websocketpp::fail_handler         fail_handler;
        websocketpp::interrupt_handler    interrupt_handler;
        websocketpp::validate_handler     validate_handler;
        typename websocketpp::connection<Config>::message_handler message_handler;
    };
    using ws_callbacks_ptr = std::shared_ptr<ws_callbacks>;

    struct ws_conn_parameter
    {
        routable_server<Config>* parent;
        websocketpp::connection_hdl hdl;
        std::shared_ptr<asio::steady_timer> ping_timer;
    };
    using ws_conn_param_ptr = std::shared_ptr<ws_conn_parameter>;

    using http_handler_ptr = std::shared_ptr<websocketpp::http_handler>;
    using http_handlers = std::vector<std::tuple<std::regex, http_handler_ptr>>;
    using ws_handlers = std::vector<std::tuple<std::regex, ws_callbacks_ptr>>;

    bool __on_ping        (const websocketpp::connection_hdl & _hdl, const std::string& _payload);
    void __on_pong        (const websocketpp::connection_hdl & _hdl, const std::string& _payload);
    void __on_pong_timeout(const websocketpp::connection_hdl & _hdl, const std::string& _payload);
    void __on_open        (const websocketpp::connection_hdl & _hdl);
    void __on_close       (const websocketpp::connection_hdl & _hdl);
    void __on_fail        (const websocketpp::connection_hdl & _hdl);
    void __on_interrupt   (const websocketpp::connection_hdl & _hdl);
    bool __on_validate    (const websocketpp::connection_hdl & _hdl);
    void __on_message     (const websocketpp::connection_hdl & _hdl, 
        const typename websocketpp::connection<Config>::message_ptr& _payload);
    void __on_http        (const websocketpp::connection_hdl & _hdl);

    bool __dispatch_request(const websocketpp::connection_hdl & _hdl, http_handlers& _handlers);

    ws_callbacks_ptr __get_callbacks(const std::string& _pattern) const;
    ws_callbacks_ptr __get_callbacks(const websocketpp::connection_hdl& _hdl);

    websocketpp::server<Config> server_;
    mutable std::mutex mutex_;
    bool internal_loop_ = false;
    int ping_interval_ms_ = -1;
    int port_ = -1;

    ws_handlers ws_handlers_;
    std::unordered_map<mgpp::mem::hashable_weak_ptr<void>, ws_callbacks_ptr> ws_cbs_table_;
    std::unordered_map<mgpp::mem::hashable_weak_ptr<void>, ws_conn_param_ptr> ws_conns_;

    http_handlers http_post_handlers_;
    http_handlers http_get_handlers_;
    
    std::shared_ptr<websocketpp::ping_handler>         ping_handler_;
    std::shared_ptr<websocketpp::pong_handler>         pong_handler_;
    std::shared_ptr<websocketpp::pong_timeout_handler> pong_timeout_handler_;
};

template <typename Config>
inline routable_server<Config>::routable_server()
{
    server_.set_ping_handler([this](auto const& _connection, auto const& _payload) 
    {
        return __on_ping(_connection, _payload);
    });

    server_.set_pong_handler([this](auto const& _connection, auto const& _payload) 
    {
        __on_pong(_connection, _payload);
    });

    server_.set_pong_timeout_handler([this](auto const& _connection, auto const& _payload) 
    {
        __on_pong_timeout(_connection, _payload);
    });

    server_.set_open_handler([this](auto const& _connection) 
    {
        __on_open(_connection);
    });

    server_.set_close_handler([this](auto const& _connection) 
    {
        __on_close(_connection);
    });

    server_.set_fail_handler([this](auto const& _connection) 
    {
        __on_fail(_connection);
    });

    server_.set_interrupt_handler([this](auto const& _connection) 
    {
        __on_interrupt(_connection);
    });

    server_.set_validate_handler([this](auto const& _connection) 
    {
        return __on_validate(_connection);
    });

    server_.set_message_handler([this](auto const& _connection, auto const& _payload) 
    {
        __on_message(_connection, _payload);
    });

    server_.set_http_handler([this](auto const& _connection) 
    {
        __on_http(_connection);
    });
}

template<typename Config>
inline void routable_server<Config>::set_port(uint16_t _port)
{
    port_ = _port;
}

template<typename Config>
inline mgpp::err routable_server<Config>::init_asio(websocketpp::lib::asio::io_service* _io_service)
{
    websocketpp::lib::asio::error_code ec;
    source().init_asio(_io_service, ec);
    internal_loop_ = (!_io_service);
    if (ec) {
        return mgpp::err{ mgec__from_sys_err(ec.value()), mm_from(ec.message()) };
    }
    return {};
}

template<typename Config>
inline mgpp::err routable_server<Config>::start()
{
    if (internal_loop_) {
        server_.reset();
    }
#if !MMBKPP_OPT__IPV6_ENABLED
    server_.listen(websocketpp::lib::asio::ip::tcp::v4(), uint16_t(port_));
#else
    server_.listen(uint16_t(port_));
#endif
    server_.start_accept();
    return {};
}

template<typename Config>
inline mgpp::err routable_server<Config>::run()
{
    if (internal_loop_) {
        server_.reset();

        server_.run();
        return {};
    }
    else {
        return mgpp::err{ MGEC__ERR, "server is not initialized with internal event loop" };
    }
}

template<typename Config>
inline mgpp::err routable_server<Config>::stop_request()
{
    server_.get_io_service().post([this]
    {
        server_.stop_listening();

        std::string reason = "server stop";
        std::error_code ec;
    
        std::unique_lock<std::mutex> locker(mutex_);
        for (auto& conn : ws_conns_) {
            ec.clear();
            
            server_.pause_reading(conn.second->hdl, ec);
            if (ec) {
                // TO_DO
            }
            server_.close(conn.second->hdl, websocketpp::close::status::going_away, reason, ec);
            if (ec) {
                // TO_DO
            }
        }
    });
    
    return {};
}

template <typename Config>
inline routable_server<Config>& routable_server<Config>::http_post(std::string const& _pattern, websocketpp::http_handler const& _handler) 
{
    if (!_handler) {
        return *this;
    }
    if (_pattern.empty()) {
        return http_post(".*", _handler);
    }
    std::unique_lock<std::mutex> locker(mutex_);
    http_post_handlers_.emplace_back(std::make_tuple(std::regex(_pattern), std::make_shared<websocketpp::http_handler>(_handler)));
    return *this;
}

template <typename Config>
inline routable_server<Config>& routable_server<Config>::http_get(std::string const& _pattern, websocketpp::http_handler const& _handler) 
{
    if (!_handler) {
        return *this;
    }
    if (_pattern.empty()) {
        return http_get(".*", _handler);
    }
    std::unique_lock<std::mutex> locker(mutex_);
    http_get_handlers_.emplace_back(std::make_tuple(std::regex(_pattern), std::make_shared<websocketpp::http_handler>(_handler)));
    return *this;
}

template <typename Config>
inline bool routable_server<Config>::__on_ping(const websocketpp::connection_hdl& _hdl, const std::string& _payload)
{
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        return true;
    }

    if (callbacks->ping_handler)
        return callbacks->ping_handler(_hdl, _payload);

    std::unique_lock<std::mutex> locker(mutex_);
    auto ping_handler = ping_handler_;
    locker.unlock();

    if (ping_handler)
        return (*ping_handler)(_hdl, _payload);

    return true;
}

template <typename Config>
inline void routable_server<Config>::__on_pong(const websocketpp::connection_hdl& _hdl, const std::string& _payload)
{
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        return ;
    }

    if (callbacks->pong_handler)
        callbacks->pong_handler(_hdl, _payload);
    
    std::unique_lock<std::mutex> locker(mutex_);
    auto pong_handler = pong_handler_;
    locker.unlock();

    if (pong_handler)
        (*pong_handler)(_hdl, _payload);
}

template <typename Config>
inline void routable_server<Config>::__on_pong_timeout(const websocketpp::connection_hdl& _hdl, const std::string& _payload)
{
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        return ;
    }

    if (callbacks->pong_timeout_handler)
        callbacks->pong_timeout_handler(_hdl, _payload);

    std::unique_lock<std::mutex> locker(mutex_);
    auto pong_timeout_handler = pong_timeout_handler_;
    locker.unlock();

    if (pong_timeout_handler)
        (*pong_timeout_handler)(_hdl, _payload);
}

template <typename Config>
inline void routable_server<Config>::__on_open(const websocketpp::connection_hdl& _hdl)
{    
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        server_.close(_hdl, websocketpp::close::status::policy_violation, "not found path");
        return ;
    }

    auto conn = server_.get_con_from_hdl(_hdl);
    
    if (callbacks->open_handler) {
        auto ping_timer = conn->set_timer(ping_interval_ms_, [this, _hdl](auto const& _hdl) 
        {
            // TO_DO
        });
        auto wscp = std::make_shared<ws_conn_parameter>();
        wscp->hdl = _hdl;
        wscp->ping_timer = ping_timer;
        std::unique_lock<std::mutex> locker(mutex_);
        ws_conns_.emplace(_hdl, wscp);
        locker.unlock();
        callbacks->open_handler(_hdl);
    }
    else
        conn->close(websocketpp::close::status::policy_violation, "not handler");
}

template <typename Config>
inline void routable_server<Config>::__on_close(const websocketpp::connection_hdl& _hdl)
{
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        return ;
    }

    if (callbacks->close_handler)
        callbacks->close_handler(_hdl);

    std::unique_lock<std::mutex> locker(mutex_);
    ws_cbs_table_.erase(_hdl);
    ws_conns_.erase(_hdl);
}

template <typename Config>
inline void routable_server<Config>::__on_fail(const websocketpp::connection_hdl& _hdl)
{
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        return ;
    }

    if (callbacks->fail_handler)
        callbacks->fail_handler(_hdl);

    std::unique_lock<std::mutex> locker(mutex_);
    ws_cbs_table_.erase(_hdl);
}

template <typename Config>
inline void routable_server<Config>::__on_interrupt(const websocketpp::connection_hdl& _hdl)
{
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        return ;
    }

    if (callbacks->interrupt_handler)
        callbacks->interrupt_handler(_hdl);
}

template <typename Config>
inline bool routable_server<Config>::__on_validate(const websocketpp::connection_hdl& _hdl)
{
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        return false;
    }

    if (callbacks->validate_handler)
        return callbacks->validate_handler(_hdl);

    return true;
}

template <typename Config>
inline void routable_server<Config>::__on_message(
    const websocketpp::connection_hdl& _hdl, 
    const typename websocketpp::connection<Config>::message_ptr& _payload)
{
    auto callbacks = __get_callbacks(_hdl);
    if (!callbacks) {
        return ;
    }

    if (callbacks->message_handler)
        callbacks->message_handler(_hdl, _payload);
}

template <typename Config>
inline void routable_server<Config>::__on_http(const websocketpp::connection_hdl& _hdl)
{
    // if upgrade header is found, assume this is a websocket connection; otherwise, assume it's an HTTP request
    auto conn = server_.get_con_from_hdl(_hdl);
    auto const& headers = conn->get_request_header("upgrade");
    if (headers == "websocket") {
        return ;
    }
    else {
    }
    
    auto const& method = conn->get_request().get_method();

    if (method == "POST") {
        __dispatch_request(_hdl, http_post_handlers_);
    }
    else if (method == "GET" || method == "HEAD") {
        __dispatch_request(_hdl, http_get_handlers_);
    }
    
    conn->set_status(websocketpp::http::status_code::not_found);
}

template <typename Config>
inline bool routable_server<Config>::__dispatch_request(const websocketpp::connection_hdl& _hdl, http_handlers& _handlers)
{
    auto conn = server_.get_con_from_hdl(_hdl);
    auto const& path = conn->get_resource();

    std::unique_lock<std::mutex> locker(mutex_);
    for (auto const& [pattern, handler] : _handlers) {
        if (std::regex_match(path, pattern)) {
            auto ptr = handler;
            locker.unlock();
            (*ptr)(conn);
            return true;
        }
    }
    return false;
}

template <typename Config>
inline typename routable_server<Config>::ws_callbacks_ptr 
    routable_server<Config>::__get_callbacks(const std::string& _pattern) const
{
    std::unique_lock<std::mutex> locker(mutex_);
    for (auto const& [pattern, callbacks] : ws_handlers_) 
    {
        if (std::regex_match(_pattern, pattern)) 
        {
            return callbacks;
        }
    }
    return nullptr;
}

template <typename Config>
inline typename routable_server<Config>::ws_callbacks_ptr 
    routable_server<Config>::__get_callbacks(const websocketpp::connection_hdl& _hdl)
{
    std::unique_lock<std::mutex> locker(mutex_);
    auto it = ws_cbs_table_.find(_hdl);
    if (it != ws_cbs_table_.end()) {
        return it->second;
    }
    locker.unlock();

    auto conn = server_.get_con_from_hdl(_hdl);
    auto const& path = conn->get_resource();

    locker.lock();
    for (auto const& [pattern, callbacks] : ws_handlers_) 
    {
        if (std::regex_match(path, pattern)) 
        {
            ws_cbs_table_.emplace(_hdl, callbacks);
            return callbacks;
        }
    }

    return nullptr;
}

}
}
}

#endif // !MMBKPP_WRAP_WSPP_ROUTABLE_SERVER_HPP_INCLUDED
