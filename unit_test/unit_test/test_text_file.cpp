#include <catch2/catch.hpp>

// outcome 命名空间别名，与 text_file.hpp 内部一致
#include <outcome/result.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;

#include <megopp/err/err.h>
#include <mego/err/ec.h>
#include <mmbkpp/app/text_file.hpp>

#include <ghc/filesystem.hpp>
#include <fstream>
#include <string>
#include <chrono>

// ---------------------------------------------------------------------------
// 测试用文档类型
// ---------------------------------------------------------------------------
struct TextFileDoc
{
    std::string value;
};

// ---------------------------------------------------------------------------
// 策略模板特化（放在 mmbkpp::app 命名空间中）
// ---------------------------------------------------------------------------
namespace mmbkpp::app {

template<>
struct serializer<TextFileDoc>
{
    static inline outcome::checked<std::string, mgpp::err>
    serialize(const TextFileDoc& obj)
    {
        return outcome::success(std::string("value=") + obj.value);
    }
};

template<>
struct deserializer<TextFileDoc>
{
    static inline outcome::checked<TextFileDoc, mgpp::err>
    deserialize(const std::string& str)
    {
        const std::string prefix = "value=";
        if (str.substr(0, prefix.size()) == prefix)
            return outcome::success(TextFileDoc{ str.substr(prefix.size()) });
        return outcome::failure(mgpp::err{ MGEC__ERR });
    }
};

template<>
struct default_creator<TextFileDoc>
{
    static inline TextFileDoc create()
    {
        return TextFileDoc{ "default" };
    }
};

} // namespace mmbkpp::app

// ---------------------------------------------------------------------------
// RAII 临时目录辅助
// ---------------------------------------------------------------------------
struct TempDir
{
    ghc::filesystem::path path;

    TempDir()
    {
        auto ts = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path     = ghc::filesystem::temp_directory_path()
                   / ("mmbkpp_txtfile_" + std::to_string(ts));
        ghc::filesystem::create_directories(path);
    }

    ~TempDir()
    {
        ghc::filesystem::remove_all(path);
    }
};

// ---------------------------------------------------------------------------
// 测试用例
// ---------------------------------------------------------------------------

TEST_CASE("txtfile - 路径为空时 load 返回失败", "[txtfile]")
{
    mmbkpp::app::txtfile<TextFileDoc> f;
    auto result = f.load();
    REQUIRE(result.has_error());
}

TEST_CASE("txtfile - auto_create=true 且文件不存在时创建默认文件并返回默认值", "[txtfile]")
{
    TempDir tmp;
    auto filepath = tmp.path / "default_test.txt";

    mmbkpp::app::txtfile<TextFileDoc> f;
    f.set_path(filepath);
    f.set_auto_create_flag(true);
    f.set_utf8_bom_flag(false);

    REQUIRE_FALSE(ghc::filesystem::exists(filepath));

    auto result = f.load();
    REQUIRE(result.has_value());
    REQUIRE(result.value().value == "default");
    REQUIRE(ghc::filesystem::exists(filepath)); // 文件被创建
}

TEST_CASE("txtfile - auto_create=false 且文件不存在时返回失败", "[txtfile]")
{
    TempDir tmp;
    auto filepath = tmp.path / "no_create.txt";

    mmbkpp::app::txtfile<TextFileDoc> f;
    f.set_path(filepath);
    f.set_auto_create_flag(false);
    f.set_utf8_bom_flag(false);

    auto result = f.load();
    REQUIRE(result.has_error());
}

TEST_CASE("txtfile - save 后 load 值完整往返", "[txtfile]")
{
    TempDir tmp;
    auto filepath = tmp.path / "roundtrip.txt";

    mmbkpp::app::txtfile<TextFileDoc> f;
    f.set_path(filepath);
    f.set_utf8_bom_flag(false);

    TextFileDoc original{ "hello_world" };
    auto save_err = f.save(original);
    REQUIRE(save_err.ok());

    auto loaded = f.load();
    REQUIRE(loaded.has_value());
    REQUIRE(loaded.value().value == "hello_world");
}

TEST_CASE("txtfile - 写入 UTF-8 BOM 后再 load 能正确跳过 BOM", "[txtfile]")
{
    TempDir tmp;
    auto filepath = tmp.path / "bom_test.txt";

    mmbkpp::app::txtfile<TextFileDoc> f;
    f.set_path(filepath);
    f.set_utf8_bom_flag(true);

    TextFileDoc doc{ "bom_value" };
    REQUIRE(f.save(doc).ok());

    // 验证文件头部包含 BOM
    {
        std::ifstream ifs(filepath.native(), std::ios::binary);
        char bom[3] = {};
        ifs.read(bom, 3);
        REQUIRE(static_cast<unsigned char>(bom[0]) == 0xEF);
        REQUIRE(static_cast<unsigned char>(bom[1]) == 0xBB);
        REQUIRE(static_cast<unsigned char>(bom[2]) == 0xBF);
    }

    // load 时 BOM 被跳过，解析正确
    auto loaded = f.load();
    REQUIRE(loaded.has_value());
    REQUIRE(loaded.value().value == "bom_value");
}

TEST_CASE("txtfile - 文件大小超过 max_file_size 时 load 返回失败", "[txtfile]")
{
    TempDir tmp;
    auto filepath = tmp.path / "toobig.txt";

    // 先写一个合法文件
    {
        std::ofstream ofs(filepath.native());
        ofs << "value=x";
    }

    mmbkpp::app::txtfile<TextFileDoc> f;
    f.set_path(filepath);
    f.set_auto_create_flag(false);
    f.set_utf8_bom_flag(false);
    f.set_max_file_size(3); // 比 "value=x" (7字节) 小

    auto result = f.load();
    REQUIRE(result.has_error());
}
