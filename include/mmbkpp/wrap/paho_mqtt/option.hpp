
#ifndef MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED
#define MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED

#include "paho/MQTTAsync.h"
#include <paho/MQTTClient.h>

#include <memory>

#include <memepp/string.hpp>

namespace mmbkpp {
namespace paho_mqtt {

    enum class ssl_version {
        none    = 0,
        tlsv1_0 = 1,
        tlsv1_1 = 2,
        tlsv1_2 = 3,
        tlsv1_3 = 4,
    };
    
namespace async {

    struct ssl_options
    {
        ssl_options() :
            ssl_version_(ssl_version::none)
        {}
        
        ssl_options(const ssl_options& _ssl_opt) :
            ssl_version_(ssl_version::none)
        {
            assign(_ssl_opt);
        }

        constexpr const memepp::string& trust_store() const noexcept { return trust_store_; }
        constexpr const memepp::string& key_store() const noexcept { return key_store_; }
        constexpr const memepp::string& private_key() const noexcept { return private_key_; }
        constexpr const memepp::string& private_key_password() const noexcept { return private_key_password_; }
        constexpr const memepp::string& enabled_cipher_suites() const noexcept { return enabled_cipher_suites_; }
        constexpr const memepp::string& ca_path() const noexcept { return ca_path_; }

        constexpr paho_mqtt::ssl_version ssl_version() const noexcept { return ssl_version_; }

        ssl_options& assign(const ssl_options& _ssl_opt)
        {
            trust_store_ = _ssl_opt.trust_store_;
            key_store_ = _ssl_opt.key_store_;
            private_key_ = _ssl_opt.private_key_;
            private_key_password_ = _ssl_opt.private_key_password_;
            enabled_cipher_suites_ = _ssl_opt.enabled_cipher_suites_;
            ca_path_ = _ssl_opt.ca_path_;
            ssl_version_ = _ssl_opt.ssl_version_;
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

        void set_private_key_password(const memepp::string& _private_key_password)
        {
            private_key_password_ = _private_key_password;
        }
        
        void set_enabled_cipher_suites(const memepp::string& _enabled_cipher_suites)
        {
            enabled_cipher_suites_ = _enabled_cipher_suites;
        }
        
        void set_ca_path(const memepp::string& _ca_path)
        {
            ca_path_ = _ca_path;
        }

        void set_ssl_version(paho_mqtt::ssl_version _ssl_version)
        {
            ssl_version_ = _ssl_version;
        }

        bool operator==(const ssl_options& _ssl_opt) const
        {
            return trust_store_ == _ssl_opt.trust_store_ &&
                key_store_ == _ssl_opt.key_store_ &&
                private_key_ == _ssl_opt.private_key_ &&
                private_key_password_ == _ssl_opt.private_key_password_ &&
                enabled_cipher_suites_ == _ssl_opt.enabled_cipher_suites_ &&
                ca_path_ == _ssl_opt.ca_path_ &&
                ssl_version_ == _ssl_opt.ssl_version_;
        }

        ssl_options& operator=(const ssl_options& _ssl_opt) { return assign(_ssl_opt); }

    private:
        memepp::string trust_store_;
        memepp::string key_store_;
        memepp::string private_key_;
        memepp::string private_key_password_;
        memepp::string enabled_cipher_suites_;
        memepp::string ca_path_;

        paho_mqtt::ssl_version ssl_version_;
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
            server_url_ = connect_options::convert_url(_server_url, !!ssl_opt_);
        }

        void set_ssl(const ssl_options& _ssl_opt)
        {
            ssl_opt_ = std::make_unique<ssl_options>(_ssl_opt);
            server_url_ = connect_options::convert_url(server_url_, !!ssl_opt_);
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
            server_url_ = connect_options::convert_url(server_url_, !!ssl_opt_);
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

        static memepp::string convert_url(const memepp::string& _url, bool _enable_ssl);
    private:

        std::unique_ptr<ssl_options> ssl_opt_;
        memepp::string server_url_;
        memepp::string username_;
        memepp::string password_;
    };

    struct create_native_options
    {
        create_options()
            : persistence_type_(MQTTCLIENT_PERSISTENCE_NONE)
            , raw_create_opt_(MQTTAsync_createOptions_initializer)
        {}

        create_options(int _mqtt_version)
            : persistence_type_(MQTTCLIENT_PERSISTENCE_NONE)
        {
            if (_mqtt_version == MQTTVERSION_5)
            {
                MQTTAsync_createOptions opts = MQTTAsync_createOptions_initializer5;
                memcpy(&raw_create_opt_, &opts, sizeof(opts));
            }
            else {
                MQTTAsync_createOptions opts = MQTTAsync_createOptions_initializer;
                memcpy(&raw_create_opt_, &opts, sizeof(opts));
            }
        }

        constexpr const memepp::string& client_id() const noexcept { return client_id_; }

        void set_client_id(const memepp::string& _client_id)
        {
            client_id_ = _client_id;
        }

        memepp::string client_id_;
        int persistence_type_;
        MQTTAsync_createOptions raw_create_opt_;
    };

    struct ssl_native_options
    {
        ssl_native_options();
        ssl_native_options(int _mqtt_version);
        ssl_native_options(const ssl_native_options& _ssl_opt);

        constexpr MQTTAsync_SSLOptions& raw() noexcept { return raw_ssl_opt_; }
        constexpr const MQTTAsync_SSLOptions& raw() const noexcept { return raw_ssl_opt_; }

        constexpr const memepp::string& trust_store() const noexcept { return trust_store_; }
        constexpr const memepp::string& key_store() const noexcept { return key_store_; }
        constexpr const memepp::string& private_key() const noexcept { return private_key_; }
        constexpr const memepp::string& private_key_password() const noexcept { return private_key_password_; }
        constexpr const memepp::string& enabled_cipher_suites() const noexcept { return enabled_cipher_suites_; }
        constexpr const memepp::string& ca_path() const noexcept { return ca_path_; }
        
        constexpr paho_mqtt::ssl_version ssl_version() const noexcept;

        ssl_native_options& assign(const ssl_native_options& _ssl_opt);
        ssl_native_options& assign(const ssl_options& _ssl_opt);

        void set_trust_store(const memepp::string& _trust_store);
        void set_key_store(const memepp::string& _key_store);
        void set_private_key(const memepp::string& _private_key);
        void set_private_key_password(const memepp::string& _private_key_password);
        void set_enabled_cipher_suites(const memepp::string& _enabled_cipher_suites);
        void set_ca_path(const memepp::string& _ca_path);

        void set_ssl_version(paho_mqtt::ssl_version _ssl_version);

        bool operator==(const ssl_native_options& _ssl_opt) const;

        ssl_native_options& operator=(const ssl_native_options& _ssl_opt) { return assign(_ssl_opt); }

    private:
        
        MQTTAsync_SSLOptions raw_ssl_opt_;
        memepp::string trust_store_;
        memepp::string key_store_;
        memepp::string private_key_;
        memepp::string private_key_password_;
        memepp::string enabled_cipher_suites_;
        memepp::string ca_path_;
    };

    struct connect_native_options
    {
        connect_native_options();
        connect_native_options(int _mqtt_version);
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

            server_url_ = connect_options::convert_url(server_url_, !!ssl_opt_);
        }

        connect_native_options& assign(const connect_native_options& _conn_opt);
        connect_native_options& assign(const connect_options& _conn_opt);

        connect_native_options& operator=(const connect_native_options& _conn_opt) { return assign(_conn_opt); }

        //static memepp::string convert_url(const memepp::string& _url, bool _enable_ssl);
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
        disconnect_native_options(int _mqtt_version);

        constexpr MQTTAsync_disconnectOptions& raw() noexcept { return raw_disconn_opt_; }
        constexpr const MQTTAsync_disconnectOptions& raw() const noexcept { return raw_disconn_opt_; }
        
    private:

        MQTTAsync_disconnectOptions raw_disconn_opt_;
    };

    inline ssl_native_options::ssl_native_options():
        raw_ssl_opt_(MQTTAsync_SSLOptions_initializer)
    {}

    inline ssl_native_options::ssl_native_options(int _mqtt_version)
    {
        if (_mqtt_version == MQTTVERSION_5)
        {
            MQTTAsync_SSLOptions opts = MQTTAsync_SSLOptions_initializer5;
            memcpy(&raw_ssl_opt_, &opts, sizeof(opts));
        }
        else {
            MQTTAsync_SSLOptions opts = MQTTAsync_SSLOptions_initializer;
            memcpy(&raw_ssl_opt_, &opts, sizeof(opts));
        }
    }

    inline ssl_native_options::ssl_native_options(const ssl_native_options& _ssl_opt)
    {
        assign(_ssl_opt);
    }

    inline constexpr paho_mqtt::ssl_version ssl_native_options::ssl_version() const noexcept 
    {
        switch (raw_ssl_opt_.sslVersion)
        {
        case MQTT_SSL_VERSION_DEFAULT:
            return paho_mqtt::ssl_version::none;
        case MQTT_SSL_VERSION_TLS_1_0:
            return paho_mqtt::ssl_version::tlsv1_0;
        case MQTT_SSL_VERSION_TLS_1_1:
            return paho_mqtt::ssl_version::tlsv1_1;
        case MQTT_SSL_VERSION_TLS_1_2:
            return paho_mqtt::ssl_version::tlsv1_2;
            //case MQTT_SSL_VERSION_TLS_1_3:
            //    return paho_mqtt::ssl_version::tlsv1_3;
        default:
            break;
        }
        return paho_mqtt::ssl_version::none;
    }

    inline ssl_native_options& ssl_native_options::assign(const ssl_native_options& _ssl_opt)
    {
        raw_ssl_opt_ = _ssl_opt.raw();
        
        set_trust_store(_ssl_opt.trust_store());
        set_key_store(_ssl_opt.key_store());
        set_private_key(_ssl_opt.private_key());
        set_private_key_password(_ssl_opt.private_key_password());
        set_enabled_cipher_suites(_ssl_opt.enabled_cipher_suites());
        set_ca_path(_ssl_opt.ca_path());
        
        return *this;
    }

    inline ssl_native_options& ssl_native_options::assign(const ssl_options& _ssl_opt)
    {
        set_trust_store(_ssl_opt.trust_store());
        set_key_store(_ssl_opt.key_store());
        set_private_key(_ssl_opt.private_key());
        set_private_key_password(_ssl_opt.private_key_password());
        set_enabled_cipher_suites(_ssl_opt.enabled_cipher_suites());
        set_ca_path(_ssl_opt.ca_path());
        set_ssl_version(_ssl_opt.ssl_version());

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
        {
            raw_ssl_opt_.privateKey = nullptr;
            raw_ssl_opt_.privateKeyPassword = nullptr;
        }
        else {
            raw_ssl_opt_.privateKey = private_key_.data();
            raw_ssl_opt_.privateKeyPassword = private_key_password_.data();
        }
    }

    inline void ssl_native_options::set_private_key_password(const memepp::string& _private_key_password)
    {
        if (private_key_password_ == _private_key_password)
            return;
        private_key_password_ = _private_key_password;
        
        if (raw_ssl_opt_.privateKey)
            raw_ssl_opt_.privateKeyPassword = private_key_password_.data();
        else
            raw_ssl_opt_.privateKeyPassword = nullptr;
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
    
    inline void ssl_native_options::set_ssl_version(paho_mqtt::ssl_version _ssl_version)
    {
        switch (_ssl_version)
        {
        case paho_mqtt::ssl_version::none:
            raw_ssl_opt_.sslVersion = MQTT_SSL_VERSION_DEFAULT;
            break;
        case paho_mqtt::ssl_version::tlsv1_0:
            raw_ssl_opt_.sslVersion = MQTT_SSL_VERSION_TLS_1_0;
            break;
        case paho_mqtt::ssl_version::tlsv1_1:
            raw_ssl_opt_.sslVersion = MQTT_SSL_VERSION_TLS_1_1;
            break;
        case paho_mqtt::ssl_version::tlsv1_2:
            raw_ssl_opt_.sslVersion = MQTT_SSL_VERSION_TLS_1_2;
            break;
            //case paho_mqtt::ssl_version::tlsv1_3:
            //    raw_ssl_opt_.sslVersion = MQTT_SSL_VERSION_TLS_1_3;
            //    break;
        default:
            raw_ssl_opt_.sslVersion = MQTT_SSL_VERSION_DEFAULT;
            break;
        }
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
            private_key_password_ == _ssl_opt.private_key_password() &&
            enabled_cipher_suites_ == _ssl_opt.enabled_cipher_suites() &&
            ca_path_ == _ssl_opt.ca_path();
    }

    inline connect_native_options::connect_native_options():
        raw_conn_opt_(MQTTAsync_connectOptions_initializer)
    {}

    inline connect_native_options::connect_native_options(int _mqtt_version)
    {
        if (_mqtt_version == MQTTVERSION_5)
        {
            MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer5;
            memcpy(&raw_conn_opt_, &opts, sizeof(opts));
        }
        else {
            MQTTAsync_connectOptions opts = MQTTAsync_connectOptions_initializer;
            memcpy(&raw_conn_opt_, &opts, sizeof(opts));
        }
    }
    
    inline connect_native_options::connect_native_options(const connect_native_options& _conn_opt)
    {
        assign(_conn_opt);
    }

    inline void connect_native_options::set_server_url(const memepp::string& _server_url)
    {
        if (server_url_ == _server_url)
            return;
        
        server_url_ = connect_options::convert_url(_server_url, !!ssl_opt_);
    }

    inline void connect_native_options::set_ssl_default()
    {
        ssl_opt_ = std::make_unique<ssl_native_options>();
        raw_conn_opt_.ssl = &(ssl_opt_->raw());

        server_url_ = connect_options::convert_url(server_url_, !!ssl_opt_);
    }

    inline void connect_native_options::set_ssl(const ssl_native_options& _ssl_opt)
    {
        if (ssl_opt_ && *ssl_opt_ == _ssl_opt)
            return;
        
        ssl_opt_ = std::make_unique<ssl_native_options>(_ssl_opt);
        raw_conn_opt_.ssl = &(ssl_opt_->raw());
        
        server_url_ = connect_options::convert_url(server_url_, !!ssl_opt_);

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

    inline memepp::string connect_options::convert_url(const memepp::string& _url, bool _enable_ssl)
    {
        if (_enable_ssl) {
            if (_url.starts_with("tcp:"))
            {
                return _url.replace("tcp:", "ssl:");
            }
            else if (_url.starts_with("ws:"))
            {
                return _url.replace("ws:", "wss:");
            }
            else {
                return _url;
            }
        }
        else {
            if (_url.starts_with("ssl:"))
            {
                return _url.replace("ssl:", "tcp:");
            }
            else if (_url.starts_with("wss:"))
            {
                return _url.replace("wss:", "ws:");
            }
            else {
                return _url;
            }
        }
    }

    inline disconnect_native_options::disconnect_native_options() :
        raw_disconn_opt_(MQTTAsync_disconnectOptions_initializer)
    {}

    inline disconnect_native_options::disconnect_native_options(int _mqtt_version)
    {
        if (_mqtt_version == MQTTVERSION_5)
        {
            MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer5;
            memcpy(&raw_disconn_opt_, &opts, sizeof(opts));
        }    
        else {
            MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
            memcpy(&raw_disconn_opt_, &opts, sizeof(opts));
        }
    }

}
} // namespace mmbkpp::paho_mqtt
} // namespace mmbkpp

#endif // !MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED
