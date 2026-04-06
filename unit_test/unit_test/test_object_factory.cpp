#include <catch2/catch.hpp>
#include <mmbkpp/app/object_factory.hpp>

#include <atomic>

namespace {

// 每组 TEST_CASE 使用独立的 base 类型，隔离各自的工厂单例状态
struct IShape   { virtual ~IShape()   = default; virtual int  type()  const = 0; };
struct IVehicle { virtual ~IVehicle() = default; virtual int  speed() const = 0; };
struct IAnimal  { virtual ~IAnimal()  = default; virtual int  legs()  const = 0; };

struct Circle : IShape   { int type()  const override { return 1; } };
struct Square : IShape   { int type()  const override { return 2; } };
struct Car    : IVehicle { int speed() const override { return 100; } };
struct Bike   : IVehicle { int speed() const override { return 20;  } };
struct Dog    : IAnimal  { int legs()  const override { return 4;   } };

using ShapeFactory   = mmbkpp::app::object_factory<IShape>;
using VehicleFactory = mmbkpp::app::object_factory<IVehicle>;
using AnimalFactory  = mmbkpp::app::object_factory<IAnimal>;

} // namespace

TEST_CASE("object_factory - 未知 key 返回 nullptr", "[object_factory]")
{
    auto result = ShapeFactory::instance().create("nonexistent_key");
    REQUIRE(result == nullptr);
}

TEST_CASE("object_factory - 注册后创建返回正确派生类型", "[object_factory]")
{
    ShapeFactory::instance().regi(
        "circle",
        [](void*) -> IShape* { return new Circle(); },
        [](IShape* p, void*)  { delete static_cast<Circle*>(p); });

    auto c = ShapeFactory::instance().create("circle");
    REQUIRE(c != nullptr);
    REQUIRE(c->type() == 1);
}

TEST_CASE("object_factory - 重复注册被静默忽略", "[object_factory]")
{
    // 先注册 Square
    ShapeFactory::instance().regi(
        "square",
        [](void*) -> IShape* { return new Square(); },
        [](IShape* p, void*)  { delete static_cast<Square*>(p); });

    // 再次注册 square，使用不同 creator（如果被接受则会返回 Circle）
    ShapeFactory::instance().regi(
        "square",
        [](void*) -> IShape* { return new Circle(); },
        [](IShape* p, void*)  { delete static_cast<Circle*>(p); });

    auto s = ShapeFactory::instance().create("square");
    REQUIRE(s != nullptr);
    REQUIRE(s->type() == 2); // 仍是 Square，说明第二次注册被忽略
}

TEST_CASE("object_factory - 注销后 create 返回 nullptr", "[object_factory]")
{
    VehicleFactory::instance().regi(
        "car",
        [](void*) -> IVehicle* { return new Car(); },
        [](IVehicle* p, void*)  { delete static_cast<Car*>(p); });

    auto before = VehicleFactory::instance().create("car");
    REQUIRE(before != nullptr);

    VehicleFactory::instance().unregi("car");

    auto after = VehicleFactory::instance().create("car");
    REQUIRE(after == nullptr);
}

TEST_CASE("object_factory - null creator 或 null destroyer 被拒绝", "[object_factory]")
{
    VehicleFactory::instance().regi(
        "bike_null_creator",
        nullptr,
        [](IVehicle* p, void*) { delete static_cast<Bike*>(p); });
    REQUIRE(VehicleFactory::instance().create("bike_null_creator") == nullptr);

    VehicleFactory::instance().regi(
        "bike_null_destroyer",
        [](void*) -> IVehicle* { return new Bike(); },
        nullptr);
    REQUIRE(VehicleFactory::instance().create("bike_null_destroyer") == nullptr);
}

TEST_CASE("object_factory - shared_ptr 释放时 destroyer 被调用", "[object_factory]")
{
    static std::atomic<int> destroy_count{0};

    AnimalFactory::instance().regi(
        "dog",
        [](void*) -> IAnimal* { return new Dog(); },
        [](IAnimal* p, void*) {
            ++destroy_count;
            delete static_cast<Dog*>(p);
        });

    {
        auto obj = AnimalFactory::instance().create("dog");
        REQUIRE(obj != nullptr);
        REQUIRE(destroy_count.load() == 0);
    } // shared_ptr 析构，触发 destroyer

    REQUIRE(destroy_count.load() == 1);
}

TEST_CASE("object_factory - instance() 始终返回同一个单例", "[object_factory]")
{
    auto& a = ShapeFactory::instance();
    auto& b = ShapeFactory::instance();
    REQUIRE(&a == &b);
}

TEST_CASE("object_factory - 多个 key 各自产生对应类型", "[object_factory]")
{
    VehicleFactory::instance().regi(
        "multi_car",
        [](void*) -> IVehicle* { return new Car();  },
        [](IVehicle* p, void*)  { delete static_cast<Car*>(p);  });
    VehicleFactory::instance().regi(
        "multi_bike",
        [](void*) -> IVehicle* { return new Bike(); },
        [](IVehicle* p, void*)  { delete static_cast<Bike*>(p); });

    auto car  = VehicleFactory::instance().create("multi_car");
    auto bike = VehicleFactory::instance().create("multi_bike");
    REQUIRE(car  != nullptr);
    REQUIRE(bike != nullptr);
    REQUIRE(car->speed()  == 100);
    REQUIRE(bike->speed() == 20);
}
