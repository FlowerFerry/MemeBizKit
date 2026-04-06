#include <catch2/catch.hpp>
#include <mmbkpp/app/text_file_monitor.hpp>
#include <mmbkpp/wrap/uvw/loop.h>

#include <ghc/filesystem.hpp>
#include <memepp/string.hpp>
#include <memepp/convert/std/string.hpp>
#include <memepp/convert/common.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>

// ---------------------------------------------------------------------------
// 被监控的文档类型（不需要策略特化，使用回调方式）
// ---------------------------------------------------------------------------
struct MonitorDoc
{
    std::string content;
};

using Monitor = mmbkpp::app::txtfile_monitor<MonitorDoc>;

// ---------------------------------------------------------------------------
// RAII 临时目录
// ---------------------------------------------------------------------------
struct MonitorTempDir
{
    ghc::filesystem::path path;

    MonitorTempDir()
    {
        auto ts = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path     = ghc::filesystem::temp_directory_path()
                   / ("mmbkpp_monitor_" + std::to_string(ts));
        ghc::filesystem::create_directories(path);
    }

    ~MonitorTempDir()
    {
        ghc::filesystem::remove_all(path);
    }
};

// ---------------------------------------------------------------------------
// 辅助：将 ghc::filesystem::path 转为 memepp::string
// ---------------------------------------------------------------------------
static memepp::string to_mm(const ghc::filesystem::path& p)
{
    return mm_from(p.string());
}

// ---------------------------------------------------------------------------
// 测试用例
// ---------------------------------------------------------------------------

TEST_CASE("txtfile_monitor - 未设置 filepath 时 start() 返回 -1", "[txtfile_monitor]")
{
    Monitor m;
    int result = m.start();
    REQUIRE(result == -1);
}

TEST_CASE("txtfile_monitor - start/stop 生命周期完成无崩溃", "[txtfile_monitor]")
{
    MonitorTempDir tmp;
    auto filepath = tmp.path / "lifecycle.txt";

    // 预先创建文件
    {
        std::ofstream ofs(filepath.native());
        ofs << "init";
    }

    Monitor m;
    m.set_filepath(to_mm(filepath));
    m.set_parse_callback([](const memepp::string_view& data, std::shared_ptr<MonitorDoc>& doc) -> int {
        doc->content = std::string(data.data(), data.size());
        return 1;
    });

    int start_result = m.start();
    REQUIRE(start_result == 0);

    // 正常 stop
    int stop_result = m.stop();
    REQUIRE(stop_result == 0);
}

TEST_CASE("txtfile_monitor - 文件变更触发 change_cb", "[txtfile_monitor]")
{
    MonitorTempDir tmp;
    auto filepath = tmp.path / "watch.txt";

    // 创建初始文件
    {
        std::ofstream ofs(filepath.native());
        ofs << "initial";
    }

    Monitor m;
    m.set_filepath(to_mm(filepath));

    m.set_parse_callback([](const memepp::string_view& data, std::shared_ptr<MonitorDoc>& doc) -> int {
        doc->content = std::string(data.data(), data.size());
        return 1;
    });

    std::atomic<int> change_count{0};
    std::string last_content;
    std::mutex content_mutex;

    m.set_change_callback("test_cb",
        [&](const std::shared_ptr<MonitorDoc>& doc) {
            std::lock_guard<std::mutex> lk(content_mutex);
            last_content = doc->content;
            ++change_count;
        });

    REQUIRE(m.start() == 0);

    // 修改文件内容，期待触发 change_cb
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::ofstream ofs(filepath.native(), std::ios::trunc);
        ofs << "updated_content";
    }

    // 等待 libuv 文件系统事件 + uvw 回调（最多 2 秒）
    const int max_wait_ms = 2000;
    const int step_ms     = 50;
    int waited = 0;
    while (change_count.load() == 0 && waited < max_wait_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
        waited += step_ms;
    }

    m.stop();

    REQUIRE(change_count.load() >= 1);
    {
        std::lock_guard<std::mutex> lk(content_mutex);
        REQUIRE(last_content == "updated_content");
    }
}

TEST_CASE("txtfile_monitor - stop 时不死锁", "[txtfile_monitor]")
{
    MonitorTempDir tmp;
    auto filepath = tmp.path / "nodeadlock.txt";

    {
        std::ofstream ofs(filepath.native());
        ofs << "data";
    }

    Monitor m;
    m.set_filepath(to_mm(filepath));
    m.set_parse_callback([](const memepp::string_view& data, std::shared_ptr<MonitorDoc>& doc) -> int {
        doc->content = std::string(data.data(), data.size());
        return 1;
    });

    REQUIRE(m.start() == 0);

    // stop 应当在有限时间内返回（join 线程）
    auto t0     = std::chrono::steady_clock::now();
    m.stop();
    auto elapsed = std::chrono::steady_clock::now() - t0;

    // 宽泛上限：5 秒内必须完成
    REQUIRE(elapsed < std::chrono::seconds(5));
}
