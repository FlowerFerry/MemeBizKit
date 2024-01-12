
#ifndef MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED
#define MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED

#include "option.hpp"

#include <megopp/err/err.h>

#include <mutex>
#include <functional>
#include <variant>

#include <outcome/result.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;

namespace mmbkpp {
namespace paho_mqtt {
namespace async {

class uvbasic_client
{
    uvbasic_client(const create_native_options& _opts)
        : create_opts_(_opts),
        , conn_opts_(_opts.raw().MQTTVersion)
        , disconn_opts_(_opts.raw().MQTTVersion)
    {
    }

    mgpp::err init();

public:
    typedef void message_arrived_cb_t(const char*, int, MQTTAsync_message*);
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

    using message_arrived_callback = std::function<message_arrived_cb_t>;
    using delivery_complete_callback = std::function<delivery_complete_cb_t>;

    using connect_lost_callback = std::function<connect_lost_cb_t>;
    using connected_callback = std::function<connected_cb_t>;
    using disconnected_callback = std::function<disconnected_cb_t>;

    using update_connect_options_callback = std::function<update_connect_options_cb_t>;

    using success_callback = std::function<void(int _mqtt_version, const success_data_t&)>;
    using failure_callback = std::function<void(int _mqtt_version, const failure_data_t&)>;

    using connect_success_callback = std::function<void(int _mqtt_version, const success_data_t&)>;
    using connect_failure_callback = std::function<void(int _mqtt_version, const failure_data_t&)>;

    using subscribe_success_callback = std::function<void(int _mqtt_version, const success_data_t&)>;
    using subscribe_failure_callback = std::function<void(int _mqtt_version, const failure_data_t&)>;

    using unsubscribe_success_callback = std::function<void(int _mqtt_version, const success_data_t&)>;
    using unsubscribe_failure_callback = std::function<void(int _mqtt_version, const failure_data_t&)>;

    using publish_success_callback = std::function<void(int _mqtt_version, const success_data_t&)>;
    using publish_failure_callback = std::function<void(int _mqtt_version, const failure_data_t&)>;

    using ssl_error_callback = std::function<ssl_error_cb_t>;
    using ssl_psk_callback = std::function<ssl_psk_cb_t>;

    ~uvbasic_client();

    // void set_message_arrived_callback

    static outcome::checked<std::shared_ptr<uvbasic_client>, mgpp::err> create(const create_native_options& _opts);

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

protected:

    mutable std::mutex mtx_;

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

    std::shared_ptr<subscribe_success_callback> subscribe_success_cb_;
    std::shared_ptr<subscribe_failure_callback> subscribe_failure_cb_;

    std::shared_ptr<unsubscribe_success_callback> unsubscribe_success_cb_;
    std::shared_ptr<unsubscribe_failure_callback> unsubscribe_failure_cb_;

    std::shared_ptr<publish_success_callback> publish_success_cb_;
    std::shared_ptr<publish_failure_callback> publish_failure_cb_;

    std::shared_ptr<ssl_error_callback> ssl_error_cb_;
    std::shared_ptr<ssl_psk_callback> ssl_psk_cb_;

};

mgpp::err uvbasic_client::init()
{

}

outcome::checked<std::shared_ptr<uvbasic_client>, mgpp::err>
    uvbasic_client::create(const create_native_options& _opts)
{
    std::shared_ptr<uvbasic_client> cli(new uvbasic_client(_opts));
    auto e = cli->init();
    if (e) {
        return e;
    }
    return cli;
}

}
}
}

#endif // !MMBKPP_WRAP_PAHOMQTT_ASYNC_CLIENT_H_INCLUDED
