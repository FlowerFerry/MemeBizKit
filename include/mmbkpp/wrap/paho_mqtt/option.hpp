
#ifndef MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED
#define MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED

#include "paho/MQTTAsync.h"
#include <paho/MQTTClient.h>

#include <memory>

#include <memepp/string.hpp>

namespace mmbkpp {
namespace paho_mqtt {
namespace async {

    struct ssl_options
    {
        ssl_options() 
        {}
        
        ssl_options(const ssl_options& _ssl_opt)
        {
            assign(_ssl_opt);
        }

        constexpr const memepp::string& trust_store() const noexcept { return trust_store_; }
        constexpr const memepp::string& key_store() const noexcept { return key_store_; }
        constexpr const memepp::string& private_key() const noexcept { return private_key_; }
        constexpr const memepp::string& enabled_cipher_suites() const noexcept { return enabled_cipher_suites_; }
        constexpr const memepp::string& ca_path() const noexcept { return ca_path_; }

        ssl_options& assign(const ssl_options& _ssl_opt)
        {
            trust_store_ = _ssl_opt.trust_store_;
            key_store_ = _ssl_opt.key_store_;
            private_key_ = _ssl_opt.private_key_;
            enabled_cipher_suites_ = _ssl_opt.enabled_cipher_suites_;
            ca_path_ = _ssl_opt.ca_path_;
            return *this;
        }

        void set_trust_store(const memepp::string& _trust_store)
        {
            trust_store_ = _trust_store;
        }
        
        void set_key_store(const memepp::string& _key_store)
        {
            key_store_ = _key_store;
        }

        void set_private_key(const memepp::string& _private_key)
        {
            private_key_ = _private_key;
        }
        
        void set_enabled_cipher_suites(const memepp::string& _enabled_cipher_suites)
        {
            enabled_cipher_suites_ = _enabled_cipher_suites;
        }
        
        void set_ca_path(const memepp::string& _ca_path)
        {
            ca_path_ = _ca_path;
        }

        bool operator==(const ssl_options& _ssl_opt) const
        {
            return trust_store_ == _ssl_opt.trust_store_ &&
                key_store_ == _ssl_opt.key_store_ &&
                private_key_ == _ssl_opt.private_key_ &&
                enabled_cipher_suites_ == _ssl_opt.enabled_cipher_suites_ &&
                ca_path_ == _ssl_opt.ca_path_;
        }

        ssl_options& operator=(const ssl_options& _ssl_opt) { return assign(_ssl_opt); }

    private:
        memepp::string trust_store_;
        memepp::string key_store_;
        memepp::string private_key_;
        memepp::string enabled_cipher_suites_;
        memepp::string ca_path_;
    };

    struct connect_options
    {

        connect_options()
        {}
        
        connect_options(const connect_options& _conn_opt)
        {
            assign(_conn_opt);
        }
        
        ssl_options* ssl() noexcept { return ssl_opt_.get(); }
        const ssl_options* ssl() const noexcept { return ssl_opt_.get(); }

        constexpr const memepp::string& server_url() const noexcept { return server_url_; }

        constexpr const memepp::string& username() const noexcept { return username_; }
        constexpr const memepp::string& password() const noexcept { return password_; }

        void set_server_url(const memepp::string& _server_url)
        {
            server_url_ = _server_url;
        }

        void set_ssl(const ssl_options& _ssl_opt)
        {
            ssl_opt_ = std::make_unique<ssl_options>(_ssl_opt);
        }
        
        void set_username(const memepp::string& _username)
        {
            username_ = _username;
        }
        
        void set_password(const memepp::string& _password)
        {
            password_ = _password;
        }

        void ssl_clear() noexcept {
            ssl_opt_.reset();
        }

        connect_options& assign(const connect_options& _conn_opt)
        {
            server_url_ = _conn_opt.server_url_;
            username_ = _conn_opt.username_;
            password_ = _conn_opt.password_;
            if (_conn_opt.ssl_opt_)
                ssl_opt_ = std::make_unique<ssl_options>(*_conn_opt.ssl_opt_);
            else
                ssl_opt_.reset();
            return *this;
        }

        connect_options& operator=(const connect_options& _conn_opt) { return assign(_conn_opt); }

    private:

        std::unique_ptr<ssl_options> ssl_opt_;
        memepp::string server_url_;
        memepp::string username_;
        memepp::string password_;
    };

    struct ssl_native_options
    {
        ssl_native_options();
        ssl_native_options(const ssl_native_options& _ssl_opt);

        constexpr MQTTAsync_SSLOptions& raw() noexcept { return raw_ssl_opt_; }
        constexpr const MQTTAsync_SSLOptions& raw() const noexcept { return raw_ssl_opt_; }

        constexpr const memepp::string& trust_store() const noexcept { return trust_store_; }
        constexpr const memepp::string& key_store() const noexcept { return key_store_; }
        constexpr const memepp::string& private_key() const noexcept { return private_key_; }
        constexpr const memepp::string& enabled_cipher_suites() const noexcept { return enabled_cipher_suites_; }
        constexpr const memepp::string& ca_path() const noexcept { return ca_path_; }

        ssl_native_options& assign(const ssl_native_options& _ssl_opt);
        ssl_native_options& assign(const ssl_options& _ssl_opt);

        void set_trust_store(const memepp::string& _trust_store);
        void set_key_store(const memepp::string& _key_store);
        void set_private_key(const memepp::string& _private_key);
        void set_enabled_cipher_suites(const memepp::string& _enabled_cipher_suites);
        void set_ca_path(const memepp::string& _ca_path);

        bool operator==(const ssl_native_options& _ssl_opt) const;

        ssl_native_options& operator=(const ssl_native_options& _ssl_opt) { return assign(_ssl_opt); }

    private:
        
        MQTTAsync_SSLOptions raw_ssl_opt_;
        memepp::string trust_store_;
        memepp::string key_store_;
        memepp::string private_key_;
        memepp::string enabled_cipher_suites_;
        memepp::string ca_path_;
    };

    struct connect_native_options
    {
        connect_native_options();
        connect_native_options(const connect_native_options& _conn_opt);

        constexpr MQTTAsync_connectOptions& raw() noexcept { return raw_conn_opt_; }
        constexpr const MQTTAsync_connectOptions& raw() const noexcept { return raw_conn_opt_; }

        ssl_native_options* ssl() noexcept { return ssl_opt_.get(); }
        const ssl_native_options* ssl() const noexcept { return ssl_opt_.get(); }

        constexpr const memepp::string& server_url() const noexcept { return server_url_; }

        constexpr const memepp::string& username() const noexcept { return username_; }
        constexpr const memepp::string& password() const noexcept { return password_; }
        
        void set_server_url(const memepp::string& _server_url);

        void set_ssl_default();
        void set_ssl(const ssl_native_options& _ssl_opt);
        void set_username(const memepp::string& _username);
        void set_password(const memepp::string& _password);

        void ssl_clear() noexcept {
            ssl_opt_.reset();
            raw_conn_opt_.ssl = nullptr;
        }

        connect_native_options& assign(const connect_native_options& _conn_opt);
        connect_native_options& assign(const connect_options& _conn_opt);

        connect_native_options& operator=(const connect_native_options& _conn_opt) { return assign(_conn_opt); }
        
    private:

        MQTTAsync_connectOptions raw_conn_opt_;
        std::unique_ptr<ssl_native_options>  ssl_opt_;
        memepp::string server_url_;
        memepp::string username_;
        memepp::string password_;
    };

    struct disconnect_native_options
    {
        disconnect_native_options();

        constexpr MQTTAsync_disconnectOptions& raw() noexcept { return raw_disconn_opt_; }
        constexpr const MQTTAsync_disconnectOptions& raw() const noexcept { return raw_disconn_opt_; }
        
    private:

        MQTTAsync_disconnectOptions raw_disconn_opt_;
    };

    inline ssl_native_options::ssl_native_options():
        raw_ssl_opt_(MQTTAsync_SSLOptions_initializer)
    {}

    inline ssl_native_options::ssl_native_options(const ssl_native_options& _ssl_opt)
    {
        assign(_ssl_opt);
    }

    inline ssl_native_options& ssl_native_options::assign(const ssl_native_options& _ssl_opt)
    {
        raw_ssl_opt_ = _ssl_opt.raw();
        
        set_trust_store(_ssl_opt.trust_store());
        set_key_store(_ssl_opt.key_store());
        set_private_key(_ssl_opt.private_key());
        set_enabled_cipher_suites(_ssl_opt.enabled_cipher_suites());
        set_ca_path(_ssl_opt.ca_path());
        
        return *this;
    }

    inline ssl_native_options& ssl_native_options::assign(const ssl_options& _ssl_opt)
    {
        set_trust_store(_ssl_opt.trust_store());
        set_key_store(_ssl_opt.key_store());
        set_private_key(_ssl_opt.private_key());
        set_enabled_cipher_suites(_ssl_opt.enabled_cipher_suites());
        set_ca_path(_ssl_opt.ca_path());

        return *this;
    }
    
    inline void ssl_native_options::set_trust_store(const memepp::string& _trust_store)
    {
        if (trust_store_ == _trust_store)
            return;
        trust_store_ = _trust_store;
        if (trust_store_.empty())
            raw_ssl_opt_.trustStore = nullptr;
        else
            raw_ssl_opt_.trustStore = trust_store_.data();
    }

    inline void ssl_native_options::set_key_store(const memepp::string& _key_store)
    {
        if (key_store_ == _key_store)
            return;
        key_store_ = _key_store;
        if (key_store_.empty())
            raw_ssl_opt_.keyStore = nullptr;
        else
            raw_ssl_opt_.keyStore = key_store_.data();
    }

    inline void ssl_native_options::set_private_key(const memepp::string& _private_key)
    {
        if (private_key_ == _private_key)
            return;
        private_key_ = _private_key;
        if (private_key_.empty())
            raw_ssl_opt_.privateKey = nullptr;
        else
            raw_ssl_opt_.privateKey = private_key_.data();
    }

    inline void ssl_native_options::set_enabled_cipher_suites(const memepp::string& _enabled_cipher_suites)
    {
        if (enabled_cipher_suites_ == _enabled_cipher_suites)
            return;
        enabled_cipher_suites_ = _enabled_cipher_suites;
        if (enabled_cipher_suites_.empty())
            raw_ssl_opt_.enabledCipherSuites = nullptr;
        else
            raw_ssl_opt_.enabledCipherSuites = enabled_cipher_suites_.data();
    }

    inline void ssl_native_options::set_ca_path(const memepp::string& _ca_path)
    {
        if (ca_path_ == _ca_path)
            return;
        ca_path_ = _ca_path;
        if (ca_path_.empty())
            raw_ssl_opt_.CApath = nullptr;
        else
            raw_ssl_opt_.CApath = ca_path_.data();
    }

    inline bool ssl_native_options::operator==(const ssl_native_options& _ssl_opt) const
    {
        return raw_ssl_opt_.disableDefaultTrustStore == _ssl_opt.raw().disableDefaultTrustStore &&
            raw_ssl_opt_.enableServerCertAuth == _ssl_opt.raw().enableServerCertAuth &&
            raw_ssl_opt_.sslVersion == _ssl_opt.raw().sslVersion &&
            raw_ssl_opt_.verify == _ssl_opt.raw().verify &&
            trust_store_ == _ssl_opt.trust_store() &&
            key_store_ == _ssl_opt.key_store() &&
            private_key_ == _ssl_opt.private_key() &&
            enabled_cipher_suites_ == _ssl_opt.enabled_cipher_suites() &&
            ca_path_ == _ssl_opt.ca_path();
    }

    inline connect_native_options::connect_native_options():
        raw_conn_opt_(MQTTAsync_connectOptions_initializer)
    {}
    
    inline connect_native_options::connect_native_options(const connect_native_options& _conn_opt)
    {
        assign(_conn_opt);
    }

    inline void connect_native_options::set_server_url(const memepp::string& _server_url)
    {
        if (server_url_ == _server_url)
            return;
        
        if (ssl_opt_) {
            if (_server_url.starts_with("tcp:"))
            {
                server_url_ = _server_url.replace("tcp:", "ssl:");
            }
            else if (server_url_.starts_with("ws:"))
            {
                server_url_ = _server_url.replace("ws:", "wss:");
            }
            else {
                server_url_ = _server_url;
            }
        }
        else {
            server_url_ = _server_url;
        }
    }

    inline void connect_native_options::set_ssl_default()
    {
        ssl_opt_ = std::make_unique<ssl_native_options>();
    }

    inline void connect_native_options::set_ssl(const ssl_native_options& _ssl_opt)
    {
        if (ssl_opt_ && *ssl_opt_ == _ssl_opt)
            return;
        
        ssl_opt_ = std::make_unique<ssl_native_options>(_ssl_opt);
        raw_conn_opt_.ssl = &(ssl_opt_->raw());
        
        if (server_url_.starts_with("tcp:"))
        {
            server_url_ = server_url_.replace("tcp:", "ssl:");
        }
        else if (server_url_.starts_with("ws:"))
        {
            server_url_ = server_url_.replace("ws:", "wss:");
        }

    }

    inline void connect_native_options::set_username(const memepp::string& _username)
    {
        if (username_ == _username)
            return;
        username_ = _username;
        if (username_.empty())
            raw_conn_opt_.username = nullptr;
        else
            raw_conn_opt_.username = username_.data();
    }

    inline void connect_native_options::set_password(const memepp::string& _password)
    {
        if (password_ == _password)
            return;
        password_ = _password;
        if (password_.empty())
            raw_conn_opt_.password = nullptr;
        else
            raw_conn_opt_.password = password_.data();
    }

    inline connect_native_options& connect_native_options::assign(const connect_native_options& _conn_opt)
    {
        raw_conn_opt_ = _conn_opt.raw();
        
        if (_conn_opt.ssl())
            set_ssl(*_conn_opt.ssl());
        set_username(_conn_opt.username());
        set_password(_conn_opt.password());
        
        return *this;
    }
    
    inline connect_native_options& connect_native_options::assign(const connect_options& _conn_opt)
    {
        raw_conn_opt_ = MQTTAsync_connectOptions_initializer;

        set_server_url(_conn_opt.server_url());
        set_username  (_conn_opt.username());
        set_password  (_conn_opt.password());

        if (_conn_opt.ssl()) {
            if (ssl()) {
                ssl()->assign(*_conn_opt.ssl());
            }
            else {
                set_ssl_default();
                ssl()->assign(*_conn_opt.ssl());
            }
        }

        return *this;
    }

    inline disconnect_native_options::disconnect_native_options() :
        raw_disconn_opt_(MQTTAsync_disconnectOptions_initializer)
    {}

}
} // namespace mmbkpp::paho_mqtt
} // namespace mmbkpp

#endif // !MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED
