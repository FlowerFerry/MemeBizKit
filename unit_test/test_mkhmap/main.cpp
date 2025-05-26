#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "mmbkpp/container/mkhmap.hpp"

struct User {
    int id;
    std::string name;
    std::string email;
    int age;

    bool operator==(const User& o) const {
        return id == o.id && name == o.name && email == o.email && age == o.age;
    }
};

TEST_CASE("mkhmap: 基本插入与查找", "[mkhmap][insert][find]") {
    mmbkpp::container::mkhmap<User, int, std::string, std::string> users;

    User u1{1, "Tom", "tom@a.com", 18};
    User u2{2, "Jerry", "jerry@a.com", 20};
    User u3{3, "Alice", "alice@a.com", 22};

    REQUIRE(users.empty());
    REQUIRE(users.insert(std::make_tuple(u1.id, u1.name, u1.email), u1));
    REQUIRE(users.insert(std::make_tuple(u2.id, u2.name, u2.email), u2));
    REQUIRE(users.insert(std::make_tuple(u3.id, u3.name, u3.email), u3));
    REQUIRE(users.size() == 3);

    // 用不同主键查找
    auto it1 = users.find<0>(1);
    REQUIRE(it1 != users.end<0>());
    REQUIRE(it1->second == u1);

    auto it2 = users.find<1>("Jerry");
    REQUIRE(it2 != users.end<1>());
    REQUIRE(it2->second == u2);

    auto it3 = users.find<2>("alice@a.com");
    REQUIRE(it3 != users.end<2>());
    REQUIRE(it3->second == u3);

    // 查找不存在
    REQUIRE(users.find<0>(100) == users.end<0>());
    REQUIRE(users.find<1>("Nobody") == users.end<1>());
    REQUIRE(users.find<2>("nobody@a.com") == users.end<2>());
}

TEST_CASE("mkhmap: 唯一性约束", "[mkhmap][unique]") {
    mmbkpp::container::mkhmap<User, int, std::string, std::string> users;
    User u1{1, "Tom", "tom@a.com", 18};
    User u2{1, "Jerry", "jerry2@a.com", 20};          // id重复
    User u3{3, "Tom", "tom3@a.com", 22};              // name重复
    User u4{4, "Alice", "tom@a.com", 25};             // email重复

    REQUIRE(users.insert(std::make_tuple(u1.id, u1.name, u1.email), u1));
    REQUIRE_FALSE(users.insert(std::make_tuple(u2.id, u2.name, u2.email), u2)); // id冲突
    REQUIRE_FALSE(users.insert(std::make_tuple(u3.id, u3.name, u3.email), u3)); // name冲突
    REQUIRE_FALSE(users.insert(std::make_tuple(u4.id, u4.name, u4.email), u4)); // email冲突
    REQUIRE(users.size() == 1);
}

TEST_CASE("mkhmap: 按不同主键删除", "[mkhmap][erase]") {
    mmbkpp::container::mkhmap<User, int, std::string, std::string> users;
    User u1{1, "Tom", "tom@a.com", 18};
    User u2{2, "Jerry", "jerry@a.com", 20};

    users.insert(std::make_tuple(u1.id, u1.name, u1.email), u1);
    users.insert(std::make_tuple(u2.id, u2.name, u2.email), u2);

    // 按id删除
    REQUIRE(users.erase<0>(1) == 1);
    REQUIRE(users.find<0>(1) == users.end<0>());
    REQUIRE(users.find<1>("Tom") == users.end<1>());
    REQUIRE(users.find<2>("tom@a.com") == users.end<2>());
    REQUIRE(users.size() == 1);

    // 按email删除
    REQUIRE(users.erase<2>("jerry@a.com") == 1);
    REQUIRE(users.empty());
}

TEST_CASE("mkhmap: 迭代遍历", "[mkhmap][iterate]") {
    mmbkpp::container::mkhmap<User, int, std::string, std::string> users;
    users.insert(std::make_tuple(1, "Tom", "tom@a.com"), User{1, "Tom", "tom@a.com", 18});
    users.insert(std::make_tuple(2, "Jerry", "jerry@a.com"), User{2, "Jerry", "jerry@a.com", 22});

    // 主键0迭代
    std::set<int> ids;
    for (auto it = users.begin<0>(); it != users.end<0>(); ++it)
        ids.insert(it->second.id);

    REQUIRE(ids == std::set<int>({1, 2}));

    // 主键1迭代 
    std::set<std::string> names;
    for (auto it = users.begin<1>(); it != users.end<1>(); ++it)
        names.insert(it->second.name);
    REQUIRE(names == std::set<std::string>({"Tom", "Jerry"}));
}

TEST_CASE("mkhmap: 连续插入删除", "[mkhmap][insert][erase][stress]") {
    mmbkpp::container::mkhmap<User, int, std::string, std::string> users;
    int N = 100;
    for (int i = 0; i < N; ++i) {
        std::string name = "user" + std::to_string(i);
        std::string email = name + "@mail.com";
        users.insert(std::make_tuple(i, name, email), User{i, name, email, 20 + i});
    }
    REQUIRE(users.size() == N);

    // 按email删除前一半
    for (int i = 0; i < N / 2; ++i) {
        std::string email = "user" + std::to_string(i) + "@mail.com";
        REQUIRE(users.erase<2>(email) == 1);
    }
    REQUIRE(users.size() == N / 2);

    // 剩余的全部按id删除
    for (int i = N / 2; i < N; ++i) {
        REQUIRE(users.erase<0>(i) == 1);
    }
    REQUIRE(users.empty());
}

TEST_CASE("mkhmap: 边界和异常情况", "[mkhmap][edge]") {
    mmbkpp::container::mkhmap<User, int, std::string, std::string> users;
    REQUIRE(users.erase<0>(1234) == 0); // 删除不存在
    REQUIRE(users.insert(std::make_tuple(1, "Jack", "jack@a.com"), User{1, "Jack", "jack@a.com", 30}));
    REQUIRE(users.erase<1>("Jack") == 1); // 可以删除
    REQUIRE(users.empty());
    // 再次删除已不存在的
    REQUIRE(users.erase<1>("Jack") == 0);

    // 插入已删除的可以成功
    REQUIRE(users.insert(std::make_tuple(1, "Jack", "jack@a.com"), User{1, "Jack", "jack@a.com", 30}));
}

TEST_CASE("mkhmap: 支持const查找", "[mkhmap][const]") {
    mmbkpp::container::mkhmap<User, int, std::string, std::string> users;
    users.insert(std::make_tuple(1, "Tom", "tom@a.com"), User{1, "Tom", "tom@a.com", 18});

    const auto& cusers = users;
    auto it = cusers.find<1>("Tom");
    REQUIRE(it != cusers.end<1>());
    REQUIRE(it->second.id == 1);
}