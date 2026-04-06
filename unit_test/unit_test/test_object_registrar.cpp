#include <catch2/catch.hpp>
#include <mmbkpp/app/object_registrar.hpp>

namespace {

// 使用独立的 base 类型，避免与 test_object_factory.cpp 中的工厂单例冲突
struct IPlugin  { virtual ~IPlugin()  = default; virtual int id()      const = 0; };
struct IService { virtual ~IService() = default; virtual int version() const = 0; };

struct PluginA  : IPlugin  { int id()      const override { return 1; } };
struct PluginB  : IPlugin  { int id()      const override { return 2; } };
struct ServiceV1: IService { int version() const override { return 1; } };

using PluginFactory  = mmbkpp::app::object_factory<IPlugin>;
using ServiceFactory = mmbkpp::app::object_factory<IService>;

} // namespace

TEST_CASE("object_registrar - 构造即向工厂注册", "[object_registrar]")
{
    mmbkpp::app::object_registrar<IPlugin, PluginA> reg("plugin_a");

    auto obj = PluginFactory::instance().create("plugin_a");
    REQUIRE(obj != nullptr);
    REQUIRE(obj->id() == 1);
}

TEST_CASE("object_registrar - 多个 registrar 分别注册不同派生类型", "[object_registrar]")
{
    mmbkpp::app::object_registrar<IPlugin, PluginA> reg_a("reg_plugin_a");
    mmbkpp::app::object_registrar<IPlugin, PluginB> reg_b("reg_plugin_b");

    auto a = PluginFactory::instance().create("reg_plugin_a");
    auto b = PluginFactory::instance().create("reg_plugin_b");
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(a->id() == 1);
    REQUIRE(b->id() == 2);
}

TEST_CASE("object_registrar - 注册不同 base 类型的服务", "[object_registrar]")
{
    mmbkpp::app::object_registrar<IService, ServiceV1> reg("service_v1");

    auto svc = ServiceFactory::instance().create("service_v1");
    REQUIRE(svc != nullptr);
    REQUIRE(svc->version() == 1);
}
