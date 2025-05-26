#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <mmbkpp/container/nested_hmap.hpp>

TEST_CASE("nested_hmap basic operations", "[nested_hmap]") {
    using map_t = mmbkpp::container::nested_hmap<int, std::string, int>;
    map_t m;

    REQUIRE(m.empty());
    REQUIRE(m.size() == 0);

    SECTION("Insert and access") {
        m["foo"][42] = 123;
        REQUIRE_FALSE(m.empty());
        REQUIRE(m.size() == 1);
        REQUIRE(m["foo"][42] == 123);

        // operator[] const
        const map_t& cm = m;
        REQUIRE(cm["foo"][42] == 123);

        // Accessing non-existing key throws in const []
        REQUIRE_THROWS_AS(cm["bar"][100], std::out_of_range);

        // Non-const [] default constructs submaps
        m["bar"][100] = 456;
        REQUIRE(m["bar"][100] == 456);
        REQUIRE(m.size() == 2);
    }

    SECTION("find and iterators") {
        m["foo"][1] = 111;
        m["foo"][2] = 222;
        m["bar"][3] = 333;

        auto it = m.find("foo");
        REQUIRE(it != m.end());
        REQUIRE(it->first == "foo");

        auto submap = it->second;
        REQUIRE(submap.find(2) != submap.end());
        REQUIRE(submap.find(99) == submap.end());

        size_t count = 0;
        for (auto& pair : m) ++count;
        REQUIRE(count == 2);
    }

    SECTION("get function") {
        m["k1"][1] = 7;
        REQUIRE(m.get("k1", 1) != nullptr);
        REQUIRE(*m.get("k1", 1) == 7);

        REQUIRE(m.get("k1", 2) == nullptr);
        REQUIRE(m.get("notfound", 1) == nullptr);

        // Partial get (returns pointer to next layer)
        //auto* l1 = m.get("k1");
        //REQUIRE(l1 != nullptr);
        //REQUIRE(l1->get(1) != nullptr);
        //REQUIRE(*l1->get(1) == 7);
    }

    SECTION("value_or function") {
        m["k1"][5] = 42;
        REQUIRE(m.value_or(-1, "k1", 5) == 42);
        REQUIRE(m.value_or(-1, "k1", 6) == -1);
        REQUIRE(m.value_or(-2, "notfound", 5) == -2);
    }

    SECTION("clear function") {
        m["foo"][1] = 1;
        m["bar"][2] = 2;
        REQUIRE(m.size() == 2);
        m.clear();
        REQUIRE(m.empty());
    }
}

TEST_CASE("nested_hmap triple layer", "[nested_hmap]") {
    using map_t = mmbkpp::container::nested_hmap<std::string, int, char, double>;
    map_t m;

    m[1]['a'][3.14] = "pi";
    m[1]['b'][2.72] = "e";
    m[2]['a'][1.41] = "sqrt2";

    REQUIRE(m[1]['a'][3.14] == "pi");
    REQUIRE(m[1]['b'][2.72] == "e");
    REQUIRE(m[2]['a'][1.41] == "sqrt2");

    REQUIRE(m.get(1, 'a', 3.14) != nullptr);
    REQUIRE(*m.get(1, 'a', 3.14) == "pi");
    REQUIRE(m.value_or("none", 1, 'b', 0.0) == "none");

    //auto* l2 = m.get(1, 'a');
    //REQUIRE(l2 != nullptr);
    //REQUIRE(l2->get(3.14) != nullptr);
    //REQUIRE(*l2->get(3.14) == "pi");
}
