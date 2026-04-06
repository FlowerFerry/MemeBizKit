#include <catch2/catch.hpp>
#include <mmbkpp/app/request_handler.h>

#include <mego/util/std/time.h>
#include <mego/err/ec.h>

#include <string>
#include <tuple>

// 使用默认 null_mutex（单线程测试）
using Hdlr = mmbkpp::app::request_handler<std::string, std::string>;

// 辅助：连续 poll 多次，确保一次请求被分发出去
static void poll_once(Hdlr& h)
{
    h.poll(mgu_timestamp_get());
}

TEST_CASE("request_handler - enqueue 后 poll 触发 request_cb", "[request_handler]")
{
    Hdlr h;

    bool req_cb_called = false;
    std::string received_req;

    h.set_request_cb([&](Hdlr::request_id_t, Hdlr::retry_state_t, const std::string& req) -> mgpp::err {
        req_cb_called  = true;
        received_req   = req;
        return {};
    });

    auto [id, e] = h.enqueue("hello");
    REQUIRE(e.ok());
    REQUIRE(id != 0);
    REQUIRE_FALSE(req_cb_called);

    poll_once(h);
    REQUIRE(req_cb_called);
    REQUIRE(received_req == "hello");
}

TEST_CASE("request_handler - response_success 触发 response_cb 并传递响应", "[request_handler]")
{
    Hdlr h;

    Hdlr::request_id_t dispatched_id = 0;
    bool resp_cb_called  = false;
    std::string resp_val;

    h.set_request_cb([&](Hdlr::request_id_t id, Hdlr::retry_state_t, const std::string&) -> mgpp::err {
        dispatched_id = id;
        return {};
    });
    h.set_response_cb([&](Hdlr::request_id_t, const std::string&, const std::string* resp, const mgpp::err& err) -> mgpp::err {
        resp_cb_called = true;
        if (resp) resp_val = *resp;
        REQUIRE(err.ok());
        return {};
    });

    auto [id, e] = h.enqueue("req");
    REQUIRE(e.ok());

    poll_once(h);
    REQUIRE(dispatched_id == id);

    auto r = h.response_success(id, std::string("world"));
    REQUIRE(r.ok());
    REQUIRE(resp_cb_called);
    REQUIRE(resp_val == "world");
}

TEST_CASE("request_handler - response_failure 重试直到超过 max_retry", "[request_handler]")
{
    Hdlr h;
    h.set_max_retry(1); // 1 次重试，共 2 次分发

    int  req_cb_count  = 0;
    bool resp_cb_called = false;
    bool resp_was_error = false;

    h.set_request_cb([&](Hdlr::request_id_t, Hdlr::retry_state_t, const std::string&) -> mgpp::err {
        ++req_cb_count;
        return {};
    });
    h.set_response_cb([&](Hdlr::request_id_t, const std::string&, const std::string* resp, const mgpp::err& err) -> mgpp::err {
        resp_cb_called = true;
        resp_was_error = bool(err);
        REQUIRE(resp == nullptr);
        return {};
    });

    auto [id, e] = h.enqueue("test");
    REQUIRE(e.ok());

    // 第 1 次分发
    poll_once(h);
    REQUIRE(req_cb_count == 1);

    // 第 1 次失败：retry=0 < max_retry=1，请求重新入队
    h.response_failure(id, mgpp::err{ MGEC__ERR });
    REQUIRE_FALSE(resp_cb_called);

    // 第 2 次分发
    poll_once(h);
    REQUIRE(req_cb_count == 2);

    // 第 2 次失败：retry=1，不再 < max_retry=1，触发 response_cb
    h.response_failure(id, mgpp::err{ MGEC__ERR });
    REQUIRE(resp_cb_called);
    REQUIRE(resp_was_error);
}

TEST_CASE("request_handler - serialize 模式下第二个请求等待第一个完成", "[request_handler]")
{
    Hdlr h;
    h.set_serialize_flag(true);

    int req_dispatch_count = 0;

    h.set_request_cb([&](Hdlr::request_id_t, Hdlr::retry_state_t, const std::string&) -> mgpp::err {
        ++req_dispatch_count;
        return {};
    });

    auto [id1, e1] = h.enqueue("first");
    auto [id2, e2] = h.enqueue("second");
    REQUIRE(e1.ok());
    REQUIRE(e2.ok());

    // 第一次 poll：分发 first，wait_map 非空
    poll_once(h);
    REQUIRE(req_dispatch_count == 1);

    // 第二次 poll：serialize=true 且 wait_map 非空，不分发 second
    poll_once(h);
    REQUIRE(req_dispatch_count == 1);

    // 完成 first 后，再 poll 才能分发 second
    h.response_success(id1, std::string("ok"));
    poll_once(h);
    REQUIRE(req_dispatch_count == 2);

    h.response_success(id2, std::string("ok"));
}

TEST_CASE("request_handler - 队列满时 enqueue 返回错误", "[request_handler]")
{
    Hdlr h;
    h.set_max_queue(1);

    // 阻止请求被消费（不设置 request_cb），让队列保持满
    auto [id1, e1] = h.enqueue("a");
    REQUIRE(e1.ok());

    auto [id2, e2] = h.enqueue("b");
    REQUIRE_FALSE(e2.ok()); // 队列已满
}

TEST_CASE("request_handler - request_cb 未设置时 response_cb 收到 INVAL 错误", "[request_handler]")
{
    Hdlr h;

    bool resp_cb_called = false;
    int  resp_err_code  = 0;

    // 故意不设置 request_cb
    h.set_response_cb([&](Hdlr::request_id_t, const std::string&, const std::string* resp, const mgpp::err& err) -> mgpp::err {
        resp_cb_called = true;
        resp_err_code  = err.code();
        REQUIRE(resp == nullptr);
        return {};
    });

    h.enqueue("req");
    poll_once(h);

    REQUIRE(resp_cb_called);
    REQUIRE(resp_err_code == MGEC__INVAL);
}

TEST_CASE("request_handler - 对未知 id 调用 response_success 返回错误", "[request_handler]")
{
    Hdlr h;
    auto r = h.response_success(9999, std::string("x"));
    REQUIRE(bool(r)); // 应返回错误
}
