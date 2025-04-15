
#ifndef MMBKPP_WRAP_HV_HTTP_REQUEST_H_INCLUDED
#define MMBKPP_WRAP_HV_HTTP_REQUEST_H_INCLUDED

#include <hv/HttpMessage.h>
#include <memepp/buffer_view.hpp>
#include <memepp/string_view.hpp>
#include <memepp/convert/fmt.hpp>

namespace mmbkpp {
namespace wrap {
namespace hv {

inline void set_http_request_form_file(
    HttpRequest& _request, 
    const memepp::string_view& _boundary,
    const memepp::string_view& _form_name,
    const memepp::string_view& _file_name,
    const memepp::string_view& _file_content)
{
    _request.SetBody(fmt::format(
        "--{0}\r\n"
        "Content-Disposition: form-data; name=\"{1}\"; filename=\"{2}\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
        "{3}\r\n"
        "--{0}--\r\n",
        _boundary, _form_name, _file_name, _file_content));
    _request.SetHeader("Content-Type",
        fmt::format("multipart/form-data; boundary={}", _boundary));
}

inline void set_http_request_form_file(
    HttpRequest& _request, 
    const memepp::string_view& _boundary,
    const memepp::string_view& _form_name,
    const memepp::string_view& _file_name,
    const memepp::buffer_view& _file_content)
{
    auto file_content = memepp::string_view{ _file_content.data(), _file_content.size() };
    _request.SetBody(fmt::format(
        "--{0}\r\n"
        "Content-Disposition: form-data; name=\"{1}\"; filename=\"{2}\"\r\n"
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
        "{3}\r\n"
        "--{0}--\r\n",
        _boundary, _form_name, _file_name, file_content));
    _request.SetHeader("Content-Type", 
        fmt::format("multipart/form-data; boundary={}", _boundary));
}

}
}
}

#endif // !MMBKPP_WRAP_HV_HTTP_REQUEST_H_INCLUDED
