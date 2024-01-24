
#ifndef MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED
#define MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED

#if __has_include(<MQTTAsync.h>)
#   include <MQTTAsync.h>
#   include <MQTTClient.h>
#elif __has_include(<paho/MQTTAsync.h>)
#   include <paho/MQTTAsync.h>
#   include <paho/MQTTClient.h>
#endif

#include <memory>

#include <memepp/string.hpp>

namespace mmbkpp {
namespace paho_mqtt {

    enum class log_level {
        all,
        trace,
        debug,
        info,
        warn,
        err,
        critical,
        off,
    };

    enum class ssl_version {
        none    = 0,
        tlsv1_0 = 1,
        tlsv1_1 = 2,
        tlsv1_2 = 3,
        tlsv1_3 = 4,
    };
    
namespace async {

    struct create_options
    {
        create_options()
            : persistence_type_(MQTTCLIENT_PERSISTENCE_NONE)
        {}

        constexpr const memepp::string& client_id() const noexcept { return client_id_; }

        constexpr int persistence_type() const noexcept { return persistence_type_; }

        void set_client_id(const memepp::string& _client_id)
        {
            client_id_ = _client_id;
        }

        void set_persistence_type(int _persistence_type)
        {
            persistence_type_ = _persistence_type;
        }

        memepp::string client_id_;
        int persistence_type_;
    };

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
            : keep_alive_interval_(60)
            , clean_session_(false)
            , max_inflight_(10)
            , connect_timeout_(30)
            , request_retry_interval_(0)
            , version_(MQTTVERSION_DEFAULT)
            , auto_reconnect_(true)
            , min_reconnect_interval_(1)
            , max_reconnect_interval_(60)
            , cleanstart_v5_(0)
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

        constexpr int keep_alive_interval() const noexcept { return keep_alive_interval_; }
        constexpr bool clean_session() const noexcept { return clean_session_; }
        constexpr int max_inflight() const noexcept { return max_inflight_; }
        constexpr int connect_timeout() const noexcept { return connect_timeout_; }
        constexpr int request_retry_interval() const noexcept { return request_retry_interval_; }
        constexpr int version() const noexcept { return version_; }
        constexpr bool auto_reconnect() const noexcept { return auto_reconnect_; }
        constexpr int min_reconnect_interval() const noexcept { return min_reconnect_interval_; }
        constexpr int max_reconnect_interval() const noexcept { return max_reconnect_interval_; }
        constexpr int cleanstart_v5() const noexcept { return cleanstart_v5_; }

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

        void set_keep_alive_interval(int _keep_alive_interval)
        {
            keep_alive_interval_ = _keep_alive_interval;
        }
        
        void set_clean_session(bool _clean_session)
        {
            clean_session_ = _clean_session;
        }

        void set_max_inflight(int _max_inflight)
        {
            max_inflight_ = _max_inflight;
        }

        void set_connect_timeout(int _connect_timeout)
        {
            connect_timeout_ = _connect_timeout;
        }

        void set_request_retry_interval(int _request_retry_interval)
        {
            request_retry_interval_ = _request_retry_interval;
        }

        void set_version(int _version)
        {
            version_ = _version;
        }

        void set_auto_reconnect(bool _auto_reconnect)
        {
            auto_reconnect_ = _auto_reconnect;
        }

        void set_min_reconnect_interval(int _min_reconnect_interval)
        {
            min_reconnect_interval_ = _min_reconnect_interval;
        }

        void set_max_reconnect_interval(int _max_reconnect_interval)
        {
            max_reconnect_interval_ = _max_reconnect_interval;
        }

        void set_cleanstart_v5(int _cleanstart_v5)
        {
            cleanstart_v5_ = _cleanstart_v5;
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

        /**
          * һ�������� null ��β���ַ��������飬ָ���ͻ��˽����ӵ���Щ��������ÿ���ַ����ĸ�ʽΪ <i>protocol://host:port</i>��
          * <i>protocol</i> ������ <i>tcp</i>��<i>ssl</i>��<i>ws</i> �� <i>wss</i>��
          * ֻ���������� TLS �汾�Ŀ�ʱ������Чʹ�� TLS ǰ׺��ssl��wss����
          * ���� <i>host</i>��������
          * ָ�� IP ��ַ�����������磬Ҫ���ӵ��ڱ��ػ��������еķ�������ʹ��Ĭ�ϵ� MQTT �˿ڣ���ָ��
          * <i>tcp://localhost:1883</i>��
          */
        std::vector<memepp::string> server_urls_;
        memepp::string username_;
        memepp::string password_;
        
        /** "����"���������Ϊ��λ�������˿ͻ��˺ͷ�����֮����ͨ�ŵ����ʱ��
          * �ͻ��˻�ȷ��ÿ������������������һ����Ϣ�������硣�ڸ�ʱ�������ڣ����û���������
          * ����Ϣ���ͻ��˻ᷢ��һ���ǳ�С�� MQTT "ping" ��Ϣ�������������ȷ�ϡ�����
          * ���ʹ�ͻ����ܹ��ڲ��صȴ���ʱ��� TCP/IP ��ʱ������£���⵽���������ٿ��á�
          * �����ϣ�������κα��������Ϊ 0��
          */
        int keep_alive_interval_;

        /**
          * ���ǲ���ֵ��cleansession ���������ӺͶϿ�����ʱ���ƿͻ��˺ͷ���������Ϊ��
          * �ͻ��˺ͷ����������ֻỰ״̬��Ϣ����Щ��Ϣ����ȷ�� "����һ��" �� "��һ��"
          * ���ݣ��� "��һ��" �յ���Ϣ���Ự״̬Ҳ������ MQTT �ͻ��˴����Ķ��ġ�����ѡ���ڻỰ֮�䱣�ֻ���״̬��Ϣ��
          *
          * �� cleansession Ϊ��ʱ��״̬��Ϣ�����ӺͶϿ�ʱ���������� cleansession ����Ϊ�ٻᱣ��״̬
          * ��Ϣ����ʹ�� MQTTAsync_connect() ����һ�� MQTT �ͻ���Ӧ�ó���ʱ���ͻ���ʹ�ÿͻ��˱�ʶ���ͷ�������ַ��ʶ�����ӡ�
          * �������������ͻ��˵ĻỰ��Ϣ�Ƿ��Ѿ�����ǰ�����ӵ��������б��������������ǰ�ĻỰ��Ȼ���ڣ��� cleansession Ϊ�棬��ͻ��˺ͷ���������ǰ�ĻỰ��Ϣ�������
          * ��� cleansession Ϊ�٣���ǰ�ĻỰ���ָ������û����ǰ�ĻỰ���Ϳ�ʼһ���µĻỰ��
          */
        bool clean_session_;

        /**
          * ������˿���ͬʱ���е���Ϣ������
          */
        int max_inflight_;
        
        /**
          * ����������ӵ�ʱ����������Ϊ��λ����
          */
        int connect_timeout_;

        /**
         * �� TCP �Ự�ڼ䣬δȷ�ϵķ��������ڶ���������ԡ�
         * �� MQTT 3.1.1 ��֮��İ汾�У���������������ʱ��������Ҫ���ԡ�
         * 0 �رջỰ�е����ԣ������Ƽ������á�
         * ���Ѿ����ص���������������ֻ��Ӿ����⡣
         */
        int request_retry_interval_;

        /**
          * ������������ʹ�õ� MQTT �İ汾��
          * MQTTVERSION_DEFAULT (0) = Ĭ�ϣ��� 3.1.1 ��ʼ�����ʧ�ܣ��˻ص� 3.1
          * MQTTVERSION_3_1 (3) = ֻ���԰汾 3.1
          * MQTTVERSION_3_1_1 (4) = ֻ���԰汾 3.1.1
          */
        int version_;

        /**
          * �����Ӷ�ʧ��������Զ��������ӡ�
          */
        bool auto_reconnect_;

        /**
          * ��С���Զ��������Լ��������Ϊ��λ��ÿ��ʧ�ܵ����Զ���ӱ���
          */
        int min_reconnect_interval_;

        /**
          * �����Զ��������Լ��������Ϊ��λ��ʧ�ܵ����Բ����ټӱ���
          */
        int max_reconnect_interval_;
        
        /*
         * MQTT V5 �� cleanstart ��־��ֻ�ڻỰ��ʼʱ���״̬��
         */
        int cleanstart_v5_;

    };

    struct disconnect_options
    {
        disconnect_options()
            : timeout_(1)
            , reasonCode_(MQTTREASONCODE_SUCCESS)
        {}

        inline constexpr int timeout() const noexcept { return timeout_; }

        int timeout_;
        /**
         * MQTT V5 input properties
         */
        //MQTTProperties properties_;
        /**
         * Reason code for MQTTV5 disconnect
         */
        enum MQTTReasonCodes reasonCode_;
    };

    struct create_native_options
    {
        create_native_options()
            : persistence_type_(MQTTCLIENT_PERSISTENCE_NONE)
            , raw_create_opt_(MQTTAsync_createOptions_initializer)
        {}

        create_native_options(int _mqtt_version)
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

        constexpr MQTTAsync_createOptions& raw() noexcept { return raw_create_opt_; }
        constexpr const MQTTAsync_createOptions& raw() const noexcept { return raw_create_opt_; }

        constexpr const memepp::string& client_id() const noexcept { return client_id_; }

        constexpr int persistence_type() const noexcept { return persistence_type_; }

        void set_client_id(const memepp::string& _client_id)
        {
            client_id_ = _client_id;
        }

        void set_persistence_type(int _persistence_type)
        {
            persistence_type_ = _persistence_type;
        }

        create_native_options& assign(const create_options& _create_opt)
        {
            client_id_ = _create_opt.client_id();
            persistence_type_ = _create_opt.persistence_type();
            return *this;
        }

        memepp::string client_id_;
        int persistence_type_;
        MQTTAsync_createOptions raw_create_opt_;
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
        
        disconnect_native_options& assign(const disconnect_options& _disconn_opt);
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
        
        raw().keepAliveInterval = _conn_opt.keep_alive_interval();
        raw().cleansession = _conn_opt.clean_session();
        raw().automaticReconnect = _conn_opt.auto_reconnect();
        raw().connectTimeout = _conn_opt.connect_timeout();
        raw().retryInterval = _conn_opt.request_retry_interval();
        raw().maxInflight = _conn_opt.max_inflight();
        raw().MQTTVersion = _conn_opt.version();
        raw().minRetryInterval = _conn_opt.min_reconnect_interval();
        raw().maxRetryInterval = _conn_opt.max_reconnect_interval();
        raw().cleanstart = _conn_opt.cleanstart_v5();

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

    inline disconnect_native_options& disconnect_native_options::assign(const disconnect_options& _disconn_opt)
    {
        raw_disconn_opt_.timeout = _disconn_opt.timeout();
        return *this;
    }

}
} // namespace mmbkpp::paho_mqtt
} // namespace mmbkpp

#endif // !MMBK_WRAP_PAHOMQTT_OPTION_HPP_INCLUDED
