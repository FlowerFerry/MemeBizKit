#include <catch2/catch.hpp>
#include <mmbkpp/i18n/catalog.h>
#include <sqlite3.h>
#include <ghc/filesystem.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace mmbkpp::i18n;
namespace fs = ghc::filesystem;

// ============================================================================
// Helpers
// ============================================================================

static std::string S(const memepp::string& s)
{
    return std::string(s.data(), static_cast<size_t>(s.size()));
}

static std::atomic<int>& db_counter()
{
    static std::atomic<int> c{ 0 };
    return c;
}

static std::string make_db(const char* name, const char* sql)
{
    const int id = db_counter().fetch_add(1);
    fs::path p = fs::temp_directory_path() / ("mmbk_i18n_" + std::to_string(id) + "_" + name + ".db");
    if (fs::exists(p))
        fs::remove(p);

    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(p.string().c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    REQUIRE(rc == SQLITE_OK);

    char* err_msg = nullptr;
    rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string msg = err_msg ? err_msg : "unknown error";
        sqlite3_free(err_msg);
        sqlite3_close(db);
        FAIL("sqlite3_exec failed: " + msg);
    }

    sqlite3_close(db);
    return p.string();
}

// ============================================================================
// Fixture SQL
// ============================================================================

static const char* MAIN_SQL = R"(
CREATE TABLE i18n_language (lang TEXT PRIMARY KEY, plural_rule TEXT NOT NULL DEFAULT '') WITHOUT ROWID;
CREATE TABLE i18n_message (lang TEXT NOT NULL, ctx TEXT NOT NULL DEFAULT '', msgid TEXT NOT NULL,
  form INTEGER NOT NULL DEFAULT 0, msgstr TEXT NOT NULL, PRIMARY KEY (lang, ctx, msgid, form)) WITHOUT ROWID;

INSERT INTO i18n_language VALUES ('zh-CN',''),('zh',''),('ru',''),('cs',''),('en-GB',''),
  ('pl','nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);');

INSERT INTO i18n_message VALUES
 ('zh-CN','','Open',0,'打开'),
 ('zh-CN','','File',0,'档案'),
 ('zh-CN','menu','File',0,'文件菜单'),
 ('zh-CN','','%d file',0,'%d 个文件'),
 ('zh-CN','dialog','%d item',0,'%d 项'),
 ('zh','','Save',0,'保存'),
 ('en-GB','','Color',0,'Colour'),
 ('ru','','Open',0,'Открыть'),
 ('ru','','%d file',0,'%d файл'),
 ('ru','','%d file',1,'%d файла'),
 ('ru','','%d file',2,'%d файлов'),
 ('ru','','%d dog',0,'%d собака'),
 ('pl','','%d file',0,'%d plik'),
 ('pl','','%d file',1,'%d pliki'),
 ('pl','','%d file',2,'%d plików'),
 ('cs','','%d hole',1,'%d díry'),
 ('cs','','%d hole',2,'%d děr'),
 ('cs','','%d gap',0,'%d mezera'),
 ('cs','','%d gap',2,'%d mezer');
INSERT INTO i18n_message VALUES ('zh-CN','','LONG',0, printf('%.2000c','x'));
)";

static const char* PLUGIN_SQL = R"(
CREATE TABLE i18n_language (lang TEXT PRIMARY KEY, plural_rule TEXT NOT NULL DEFAULT '') WITHOUT ROWID;
CREATE TABLE i18n_message (lang TEXT NOT NULL, ctx TEXT NOT NULL DEFAULT '', msgid TEXT NOT NULL,
  form INTEGER NOT NULL DEFAULT 0, msgstr TEXT NOT NULL, PRIMARY KEY (lang, ctx, msgid, form)) WITHOUT ROWID;
INSERT INTO i18n_language VALUES ('zh-CN','');
INSERT INTO i18n_message VALUES ('zh-CN','','Open',0,'打开插件'), ('zh-CN','','Plugin Only',0,'插件专用');
)";

// ============================================================================
// 3.3 Shared expectation table
// ============================================================================

static void check_main(const catalog& c)
{
    REQUIRE(S(c.tr(make_tag("zh-CN"), "Open")) == "打开");
    REQUIRE(S(c.tr(make_tag("zh-Hans-CN"), "Open")) == "打开");
    REQUIRE(S(c.tr(make_tag("zh_CN"), "Open")) == "打开");
    REQUIRE(S(c.tr(make_tag("zh-CN"), "Save")) == "保存");
    REQUIRE(S(c.tr(make_tag("zh-TW"), "Save")) == "保存");
    REQUIRE(S(c.tr(make_tag("zh-TW"), "Open")) == "Open");
    REQUIRE(S(c.tr(make_tag("ja"), "Open")) == "Open");
    REQUIRE(S(c.tr(make_tag("en-GB"), "Color")) == "Colour");
    REQUIRE(S(c.tr(make_tag("en-US"), "Color")) == "Color");
    REQUIRE(S(c.tr(make_tag("en"), "Color")) == "Color");
    REQUIRE(S(c.tr(make_tag(""), "Open")) == "Open");
    REQUIRE(S(c.tr(make_tag("zh-CN"), "File")) == "档案");
    REQUIRE(S(c.tr_ctx(make_tag("zh-CN"), "", "File")) == "档案");
    REQUIRE(S(c.tr_ctx(make_tag("zh-CN"), "menu", "File")) == "文件菜单");
    REQUIRE(S(c.tr_ctx(make_tag("zh-CN"), "toolbar", "File")) == "File");
    REQUIRE(S(c.tr_ctx(make_tag("zh-CN"), "menu", "Open")) == "Open");

    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 0)) == "%d файлов");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 1)) == "%d файл");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 2)) == "%d файла");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 4)) == "%d файла");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 5)) == "%d файлов");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 11)) == "%d файлов");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 12)) == "%d файлов");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 21)) == "%d файл");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 22)) == "%d файла");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 25)) == "%d файлов");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 101)) == "%d файл");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d file", "%d files", 111)) == "%d файлов");

    REQUIRE(S(c.tr_n(make_tag("ru-RU"), "%d file", "%d files", 21)) == "%d файл");
    REQUIRE(S(c.tr_n(make_tag("zh-CN"), "%d file", "%d files", 1)) == "%d 个文件");
    REQUIRE(S(c.tr_n(make_tag("zh-CN"), "%d file", "%d files", 5)) == "%d 个文件");
    REQUIRE(S(c.tr_n(make_tag("ja"), "%d file", "%d files", 1)) == "%d file");
    REQUIRE(S(c.tr_n(make_tag("ja"), "%d file", "%d files", 2)) == "%d files");
    REQUIRE(S(c.tr_ctx_n(make_tag("zh-CN"), "dialog", "%d item", "%d items", 3)) == "%d 项");
    REQUIRE(S(c.tr_ctx_n(make_tag("zh-CN"), "", "%d item", "%d items", 3)) == "%d items");
    REQUIRE(S(c.tr_ctx_n(make_tag("zh-CN"), "", "%d item", "%d items", 1)) == "%d item");

    REQUIRE(S(c.tr_n(make_tag("pl"), "%d file", "%d files", 1)) == "%d plik");
    REQUIRE(S(c.tr_n(make_tag("pl"), "%d file", "%d files", 2)) == "%d pliki");
    REQUIRE(S(c.tr_n(make_tag("pl"), "%d file", "%d files", 5)) == "%d plików");
    REQUIRE(S(c.tr_n(make_tag("pl"), "%d file", "%d files", 12)) == "%d plików");
    REQUIRE(S(c.tr_n(make_tag("pl"), "%d file", "%d files", 22)) == "%d pliki");
    REQUIRE(S(c.tr_n(make_tag("pl"), "%d file", "%d files", 25)) == "%d plików");

    REQUIRE(S(c.tr_n(make_tag("ru"), "%d dog", "%d dogs", 5)) == "%d собака");
    REQUIRE(S(c.tr_n(make_tag("ru"), "%d dog", "%d dogs", 1)) == "%d собака");

    REQUIRE(S(c.tr_n(make_tag("cs"), "%d hole", "%d holes", 1)) == "%d hole");
    REQUIRE(S(c.tr_n(make_tag("cs"), "%d hole", "%d holes", 2)) == "%d díry");
    REQUIRE(S(c.tr_n(make_tag("cs"), "%d hole", "%d holes", 5)) == "%d děr");
    REQUIRE(S(c.tr_n(make_tag("cs"), "%d gap", "%d gaps", 1)) == "%d mezera");
    REQUIRE(S(c.tr_n(make_tag("cs"), "%d gap", "%d gaps", 3)) == "%d mezera");
    REQUIRE(S(c.tr_n(make_tag("cs"), "%d gap", "%d gaps", 7)) == "%d mezer");

    const std::string long_str = S(c.tr(make_tag("zh-CN"), "LONG"));
    REQUIRE(long_str.size() == 2000);
    for (char ch : long_str)
        REQUIRE(ch == 'x');

    REQUIRE(S(c.tr(make_tag("ru"), "Missing")) == "Missing");
}

// ============================================================================
// 3.4 plural rules
// ============================================================================

TEST_CASE("plural builtin", "[i18n][plural]")
{
    auto test_family = [](plural_family f, const char* expr) {
        plural_rule r1, r2;
        REQUIRE(plural_rule::parse(memepp::string_view{ expr }, r1));
        r2 = plural_rule::builtin(f);
        REQUIRE(r1.nplurals() == r2.nplurals());
        REQUIRE(r1.family() == plural_family::expr);

        for (uint64_t n = 0; n <= 1000; ++n)
            REQUIRE(r1.index(n) == r2.index(n));
        REQUIRE(r1.index(1000000) == r2.index(1000000));
        REQUIRE(r1.index(1000001) == r2.index(1000001));
        REQUIRE(r1.index(UINT64_MAX) == r2.index(UINT64_MAX));
    };

    test_family(plural_family::one, "nplurals=1; plural=0;");
    test_family(plural_family::en, "nplurals=2; plural=n != 1;");
    test_family(plural_family::fr, "nplurals=2; plural=n > 1;");
    test_family(plural_family::ru, "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2;");
    test_family(plural_family::pl, "nplurals=3; plural=n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2;");
    test_family(plural_family::cs, "nplurals=3; plural=(n==1) ? 0 : (n>=2 && n<=4) ? 1 : 2;");
    test_family(plural_family::lt, "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n%10>=2 && (n%100<10 || n%100>=20) ? 1 : 2;");
    test_family(plural_family::lv, "nplurals=3; plural=n%10==1 && n%100!=11 ? 0 : n != 0 ? 1 : 2;");
    test_family(plural_family::ro, "nplurals=3; plural=n==1 ? 0 : (n==0 || (n%100 > 0 && n%100 < 20)) ? 1 : 2;");
    test_family(plural_family::sl, "nplurals=4; plural=n%100==1 ? 0 : n%100==2 ? 1 : n%100==3 || n%100==4 ? 2 : 3;");
    test_family(plural_family::ga, "nplurals=5; plural=n==1 ? 0 : n==2 ? 1 : n<7 ? 2 : n<11 ? 3 : 4;");
    test_family(plural_family::ar, "nplurals=6; plural=n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : n%100>=3 && n%100<=10 ? 3 : n%100>=11 ? 4 : 5;");

    // builtin spot checks
    REQUIRE(plural_rule::builtin(plural_family::en).index(0) == 1);
    REQUIRE(plural_rule::builtin(plural_family::en).index(1) == 0);
    REQUIRE(plural_rule::builtin(plural_family::en).index(2) == 1);

    REQUIRE(plural_rule::builtin(plural_family::fr).index(0) == 0);
    REQUIRE(plural_rule::builtin(plural_family::fr).index(1) == 0);
    REQUIRE(plural_rule::builtin(plural_family::fr).index(2) == 1);

    REQUIRE(plural_rule::builtin(plural_family::one).index(0) == 0);
    REQUIRE(plural_rule::builtin(plural_family::one).index(1) == 0);
    REQUIRE(plural_rule::builtin(plural_family::one).index(100) == 0);

    REQUIRE(plural_rule::builtin(plural_family::ru).index(1) == 0);
    REQUIRE(plural_rule::builtin(plural_family::ru).index(2) == 1);
    REQUIRE(plural_rule::builtin(plural_family::ru).index(5) == 2);
    REQUIRE(plural_rule::builtin(plural_family::ru).index(11) == 2);
    REQUIRE(plural_rule::builtin(plural_family::ru).index(21) == 0);
    REQUIRE(plural_rule::builtin(plural_family::ru).index(22) == 1);
    REQUIRE(plural_rule::builtin(plural_family::ru).index(111) == 2);

    REQUIRE(plural_rule::builtin(plural_family::pl).index(1) == 0);
    REQUIRE(plural_rule::builtin(plural_family::pl).index(2) == 1);
    REQUIRE(plural_rule::builtin(plural_family::pl).index(5) == 2);
    REQUIRE(plural_rule::builtin(plural_family::pl).index(21) == 2);
    REQUIRE(plural_rule::builtin(plural_family::pl).index(22) == 1);

    REQUIRE(plural_rule::builtin(plural_family::cs).index(1) == 0);
    REQUIRE(plural_rule::builtin(plural_family::cs).index(3) == 1);
    REQUIRE(plural_rule::builtin(plural_family::cs).index(5) == 2);

    REQUIRE(plural_rule::builtin(plural_family::ar).index(0) == 0);
    REQUIRE(plural_rule::builtin(plural_family::ar).index(1) == 1);
    REQUIRE(plural_rule::builtin(plural_family::ar).index(2) == 2);
    REQUIRE(plural_rule::builtin(plural_family::ar).index(3) == 3);
    REQUIRE(plural_rule::builtin(plural_family::ar).index(11) == 4);
    REQUIRE(plural_rule::builtin(plural_family::ar).index(100) == 5);
    REQUIRE(plural_rule::builtin(plural_family::ar).index(102) == 5);
    REQUIRE(plural_rule::builtin(plural_family::ar).index(103) == 3);

    REQUIRE(plural_rule::builtin(plural_family::sl).index(1) == 0);
    REQUIRE(plural_rule::builtin(plural_family::sl).index(2) == 1);
    REQUIRE(plural_rule::builtin(plural_family::sl).index(3) == 2);
    REQUIRE(plural_rule::builtin(plural_family::sl).index(5) == 3);
    REQUIRE(plural_rule::builtin(plural_family::sl).index(101) == 0);

    REQUIRE(plural_rule::builtin(plural_family::ga).index(1) == 0);
    REQUIRE(plural_rule::builtin(plural_family::ga).index(2) == 1);
    REQUIRE(plural_rule::builtin(plural_family::ga).index(3) == 2);
    REQUIRE(plural_rule::builtin(plural_family::ga).index(7) == 3);
    REQUIRE(plural_rule::builtin(plural_family::ga).index(11) == 4);

    // parse failures
    {
        plural_rule r;
        memepp::string err;
        REQUIRE(!plural_rule::parse("", r, &err));
        REQUIRE(!err.empty());
    }
    { plural_rule r; REQUIRE(!plural_rule::parse("@xx", r)); }
    { plural_rule r; REQUIRE(!plural_rule::parse("nplurals=0; plural=0;", r)); }
    { plural_rule r; REQUIRE(!plural_rule::parse("nplurals=17; plural=0;", r)); }
    { plural_rule r; REQUIRE(!plural_rule::parse("nplurals=3; plural=n+;", r)); }
    { plural_rule r; REQUIRE(!plural_rule::parse("nplurals=2; plural=(n;", r)); }
    { plural_rule r; REQUIRE(!plural_rule::parse("plural=n; nplurals=2;", r)); }
    { plural_rule r; REQUIRE(!plural_rule::parse("nplurals=2; plural=n; garbage", r)); }
    {
        plural_rule r;
        std::string deep = "nplurals=2; plural=";
        for (int i = 0; i < 40; ++i) deep += "(";
        deep += "n";
        for (int i = 0; i < 40; ++i) deep += ")";
        deep += ";";
        REQUIRE(!plural_rule::parse(
            memepp::string_view{ deep.c_str(), static_cast<memepp::string_view::size_type>(deep.size()) }, r));
    }
    { plural_rule r; REQUIRE(!plural_rule::parse("nplurals=2; plural=-n;", r)); }

    // parse successes
    { plural_rule r; REQUIRE(plural_rule::parse("@ru", r)); REQUIRE(r.family() == plural_family::ru); }
    {
        plural_rule r;
        REQUIRE(plural_rule::parse("nplurals=2;plural=n", r));
        REQUIRE(r.index(1) == 1);
        REQUIRE(r.index(5) == 1);
        REQUIRE(r.index(0) == 0);
    }
    { plural_rule r; REQUIRE(plural_rule::parse("nplurals=2; plural=n/0;", r)); REQUIRE(r.index(5) == 0); }
    { plural_rule r; REQUIRE(plural_rule::parse("nplurals=2; plural=n%0;", r)); REQUIRE(r.index(5) == 0); }
    { plural_rule r; REQUIRE(plural_rule::parse("nplurals=3; plural=n;", r)); REQUIRE(r.index(10) == 2); }
    {
        plural_rule r;
        REQUIRE(plural_rule::parse("  nplurals = 3 ;  plural = ( n == 1 ) ? 0 : ( n > 4 ? 2 : 1 ) ;  ", r));
        REQUIRE(r.index(1) == 0);
        REQUIRE(r.index(3) == 1);
        REQUIRE(r.index(9) == 2);
    }
    { plural_rule r; REQUIRE(plural_rule::parse("nplurals=2; plural=!(n==1);", r)); REQUIRE(r.index(1) == 0); REQUIRE(r.index(2) == 1); }
    {
        plural_rule r;
        REQUIRE(plural_rule::parse("nplurals=3; plural=n==1 ? 0 : n==2 ? 1 : 2;", r));
        REQUIRE(r.index(1) == 0);
        REQUIRE(r.index(2) == 1);
        REQUIRE(r.index(3) == 2);
    }

    // for_language
    REQUIRE(plural_rule::for_language(make_tag("zh")).family() == plural_family::one);
    REQUIRE(plural_rule::for_language(make_tag("zh-TW")).family() == plural_family::one);
    REQUIRE(plural_rule::for_language(make_tag("ja")).family() == plural_family::one);
    REQUIRE(plural_rule::for_language(make_tag("pt-BR")).family() == plural_family::fr);
    REQUIRE(plural_rule::for_language(make_tag("pt")).family() == plural_family::en);
    REQUIRE(plural_rule::for_language(make_tag("pt-PT")).family() == plural_family::en);
    REQUIRE(plural_rule::for_language(make_tag("fr-CA")).family() == plural_family::fr);
    REQUIRE(plural_rule::for_language(make_tag("ru")).family() == plural_family::ru);
    REQUIRE(plural_rule::for_language(make_tag("uk")).family() == plural_family::ru);
    REQUIRE(plural_rule::for_language(make_tag("pl")).family() == plural_family::pl);
    REQUIRE(plural_rule::for_language(make_tag("cs")).family() == plural_family::cs);
    REQUIRE(plural_rule::for_language(make_tag("ar")).family() == plural_family::ar);
    REQUIRE(plural_rule::for_language(make_tag("xx")).family() == plural_family::en);
    REQUIRE(plural_rule::for_language(make_tag("de")).family() == plural_family::en);

    REQUIRE(plural_rule::from_text_or_language("garbage", make_tag("ru")).family() == plural_family::ru);
    REQUIRE(plural_rule::from_text_or_language("", make_tag("fr")).family() == plural_family::fr);
}

// ============================================================================
// 3.5 fast mode
// ============================================================================

TEST_CASE("fast catalog", "[i18n][fast]")
{
    {
        fast_catalog_builder b;
        b.set_source_language(make_tag("en"));
        b.add(make_tag("zh-CN"), "", "Open", "打开");
        b.add(make_tag("zh-CN"), "menu", "File", "文件");
        b.add_plural(make_tag("ru"), "", "%d file", { "%d файл", "%d файла", "%d файлов" });
        auto fast = b.build();
        fast->set_default_language(make_tag("zh-CN"));

        REQUIRE(S(fast->tr("Open")) == "打开");
        REQUIRE(S(fast->tr(make_tag("ja-JP"), "Open")) == "Open");
        REQUIRE(S(fast->tr_ctx("menu", "File")) == "文件");
        REQUIRE(S(fast->tr_n(make_tag("ru"), "%d file", "%d files", 21)) == "%d файл");

        REQUIRE(fast->language_count() == 2);
        REQUIRE(fast->has_language(make_tag("ru")));
        REQUIRE(fast->message_count(make_tag("zh-CN")) == 2);
    }

    {
        fast_catalog_builder b;
        b.add(make_tag("zh-CN"), "", "A", "1");
        b.add(make_tag("zh-CN"), "", "A", "2");
        auto fast = b.build();
        REQUIRE(S(fast->tr(make_tag("zh-CN"), "A")) == "2");
        REQUIRE(fast->message_count(make_tag("zh-CN")) == 1);
    }

    {
        fast_catalog_builder b;
        b.add(make_tag("zh-CN"), "", "First", "第一");
        auto fast1 = b.build();
        REQUIRE(fast1->has_language(make_tag("zh-CN")));

        b.add(make_tag("ja"), "", "Second", "第二");
        auto fast2 = b.build();
        REQUIRE(fast2->has_language(make_tag("ja")));
        REQUIRE(!fast2->has_language(make_tag("zh-CN")));
    }

    {
        fast_catalog_builder b;
        b.add(make_tag("zh-CN"), "", "Open", "打开");
        auto fast = b.build();

        memepp::string_view v;
        REQUIRE(fast->find_view(make_tag("zh-Hans-CN"), "", "Open", false, 0, v));
        REQUIRE(S(v.to_string()) == "打开");
        REQUIRE(!fast->find_view(make_tag("ja"), "", "Open", false, 0, v));
    }

    {
        const long B = details::live_pool_blocks().load();
        const std::string main_db = make_db("main", MAIN_SQL);

        fast_catalog_builder b;
        REQUIRE(b.load_from_sqlite(main_db.c_str()).ok());
        auto c = b.build();

        REQUIRE(c->language_count() == 6);
        REQUIRE(details::live_pool_blocks().load() == B + 6);

        {
            auto s = c->tr(make_tag("zh-CN"), "LONG");
            auto s2 = s;
            c.reset();
            REQUIRE(details::live_pool_blocks().load() == B + 1);
            REQUIRE(S(s).size() == 2000);
            REQUIRE(S(s2).size() == 2000);

            s = memepp::string{};
            REQUIRE(details::live_pool_blocks().load() == B + 1);
            s2 = memepp::string{};
            REQUIRE(details::live_pool_blocks().load() == B);
        }

        fast_catalog_builder b2;
        REQUIRE(b2.load_from_sqlite(main_db.c_str()).ok());
        auto c2 = b2.build();
        {
            auto t = c2->tr(make_tag("zh-CN"), "Open");
            c2.reset();
            REQUIRE(details::live_pool_blocks().load() == B);
            REQUIRE(S(t) == "打开");
        }
        auto c3 = b2.build();   // keep a catalog alive to avoid dangling tables
        (void)c3;
        c3.reset();
        (void)c2;

        fs::remove(main_db);
    }

    {
        fast_catalog_builder b;
        mgpp::err e = b.load_from_sqlite("nonexistent_path_xyz.db");
        REQUIRE(!e.ok());
    }
    {
        const std::string empty_db = make_db("empty", "");
        fast_catalog_builder b;
        mgpp::err e = b.load_from_sqlite(empty_db.c_str());
        REQUIRE(!e.ok());
        fs::remove(empty_db);
    }

    {
        fast_catalog_builder b;
        b.set_plural_rule(make_tag("ru"), "@one");
        b.add_plural(make_tag("ru"), "", "%d file", { "A", "B", "C" });
        auto fast = b.build();
        REQUIRE(S(fast->tr_n(make_tag("ru"), "%d file", "%d files", 5)) == "A");
    }
    {
        fast_catalog_builder b;
        b.set_plural_rule(make_tag("de"), "nplurals=2; plural=n>=10;");
        b.add_plural(make_tag("de"), "", "x", { "S", "P" });
        auto fast = b.build();
        REQUIRE(S(fast->tr_n(make_tag("de"), "x", "xs", 5)) == "S");
        REQUIRE(S(fast->tr_n(make_tag("de"), "x", "xs", 10)) == "P");
    }
    {
        fast_catalog_builder b;
        b.set_plural_rule(make_tag("fr"), "bad");
        b.add_plural(make_tag("fr"), "", "y", { "S", "P" });
        auto fast = b.build();
        REQUIRE(S(fast->tr_n(make_tag("fr"), "y", "ys", 0)) == "S");
    }
}

// ============================================================================
// 3.6 lite mode
// ============================================================================

TEST_CASE("lite catalog", "[i18n][lite]")
{
    {
        mgpp::err e;
        auto lite = lite_catalog::open("nonexistent_xyz.db", {}, &e);
        REQUIRE(!lite);
        REQUIRE(!e.ok());
    }
    {
        const std::string empty_db = make_db("empty_lite", "");
        mgpp::err e;
        auto lite = lite_catalog::open(empty_db.c_str(), {}, &e);
        REQUIRE(!lite);
        fs::remove(empty_db);
    }

    {
        const std::string main_db = make_db("main_lite", MAIN_SQL);
        auto lite = lite_catalog::open(main_db.c_str());
        REQUIRE(lite);
        REQUIRE(lite->connection_count() == 0);
        REQUIRE(lite->has_language(make_tag("zh-CN")));
        REQUIRE(lite->language_count() == 6);
        lite.reset();
        fs::remove(main_db);
    }

    {
        const std::string main_db = make_db("main_lite2", MAIN_SQL);
        auto lite = lite_catalog::open(main_db.c_str());
        REQUIRE(lite);
        (void)S(lite->tr(make_tag("ja"), "Open"));
        REQUIRE(lite->connection_count() == 0);
        lite.reset();
        fs::remove(main_db);
    }

    {
        const std::string main_db = make_db("main_lite3", MAIN_SQL);
        auto lite = lite_catalog::open(main_db.c_str());
        REQUIRE(lite);

        std::atomic<int> ready{ 0 };
        std::atomic<bool> go{ false };
        std::atomic<int> bad{ 0 };
        std::vector<std::thread> threads;

        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&]() {
                if (S(lite->tr(make_tag("zh-CN"), "Open")) != "打开")
                    ++bad;
                ++ready;
                while (!go.load())
                    std::this_thread::yield();
            });
        }

        while (ready.load() < 8)
            std::this_thread::yield();
        REQUIRE(lite->connection_count() == 8);

        (void)S(lite->tr(make_tag("zh-CN"), "Open"));
        REQUIRE(lite->connection_count() == 9);

        lite->release_thread_connection();
        REQUIRE(lite->connection_count() == 8);

        go.store(true);
        for (auto& t : threads)
            t.join();

        REQUIRE(lite->connection_count() == 0);
        REQUIRE(bad.load() == 0);

        lite.reset();
        fs::remove(main_db);
    }

    {
        const std::string main_db = make_db("main_lite4", MAIN_SQL);
        auto lite = lite_catalog::open(main_db.c_str());
        REQUIRE(lite);

        lite_catalog* raw = lite.get();
        std::atomic<int> ready{ 0 };
        std::atomic<bool> go{ false };

        std::thread a([&]() {
            (void)S(raw->tr(make_tag("zh-CN"), "Open"));
            ++ready;
            while (!go.load())
                std::this_thread::yield();
        });

        while (ready.load() < 1)
            std::this_thread::yield();

        lite.reset();
        REQUIRE(fs::remove(main_db));

        go.store(true);
        a.join();
    }

    {
        const std::string main_db = make_db("main_lite5", MAIN_SQL);
        lite_options o;
        o.cache_capacity = 4;
        auto lite = lite_catalog::open(main_db.c_str(), o);
        REQUIRE(lite);

        (void)S(lite->tr(make_tag("zh-CN"), "Open"));
        (void)S(lite->tr(make_tag("zh-CN"), "File"));
        (void)S(lite->tr_ctx(make_tag("zh-CN"), "menu", "File"));
        (void)S(lite->tr(make_tag("zh-CN"), "Save"));
        (void)S(lite->tr(make_tag("ru"), "Nope"));

        REQUIRE(lite->thread_cache_size() == 4);

        (void)S(lite->tr(make_tag("zh-CN"), "Open"));
        (void)S(lite->tr(make_tag("zh-CN"), "File"));
        (void)S(lite->tr_ctx(make_tag("zh-CN"), "menu", "File"));
        (void)S(lite->tr(make_tag("zh-CN"), "Save"));
        (void)S(lite->tr(make_tag("ru"), "Nope"));

        check_main(*lite);
        lite.reset();

        lite_options o2;
        o2.cache_capacity = 0;
        auto lite2 = lite_catalog::open(main_db.c_str(), o2);
        REQUIRE(lite2);
        (void)S(lite2->tr(make_tag("zh-CN"), "Open"));
        REQUIRE(lite2->thread_cache_size() == 0);

        lite2.reset();
        fs::remove(main_db);
    }

    {
        const std::string main_db = make_db("main_lite6", MAIN_SQL);
        auto lite = lite_catalog::open(main_db.c_str());
        REQUIRE(lite);

        std::vector<std::thread> threads;
        std::atomic<int> bad{ 0 };

        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&, i]() {
                for (int j = 0; j < 2000; ++j) {
                    const int k = (i * 2000 + j) % 6;
                    std::string result;
                    if (k == 0)
                        result = S(lite->tr(make_tag("zh-CN"), "Open"));
                    else if (k == 1)
                        result = S(lite->tr(make_tag("zh-TW"), "Open"));
                    else if (k == 2)
                        result = S(lite->tr_ctx(make_tag("zh-CN"), "menu", "File"));
                    else if (k == 3)
                        result = S(lite->tr_n(make_tag("ru"), "%d file", "%d files", 21));
                    else if (k == 4)
                        result = S(lite->tr_n(make_tag("pl"), "%d file", "%d files", 22));
                    else
                        result = S(lite->tr_n(make_tag("cs"), "%d hole", "%d holes", 1));

                    bool ok = false;
                    if (k == 0) ok = (result == "打开");
                    else if (k == 1) ok = (result == "Open");
                    else if (k == 2) ok = (result == "文件菜单");
                    else if (k == 3) ok = (result == "%d файл");
                    else if (k == 4) ok = (result == "%d pliki");
                    else ok = (result == "%d hole");

                    if (!ok)
                        ++bad;
                }
            });
        }

        for (auto& t : threads)
            t.join();

        REQUIRE(bad.load() == 0);
        REQUIRE(lite->connection_count() == 0);

        lite.reset();
        fs::remove(main_db);
    }
}

// ============================================================================
// 3.7 registry and global
// ============================================================================

TEST_CASE("registry", "[i18n][registry]")
{
    const std::string main_db = make_db("main_reg", MAIN_SQL);
    const std::string plugin_db = make_db("plugin_reg", PLUGIN_SQL);

    fast_catalog_builder b;
    REQUIRE(b.load_from_sqlite(main_db.c_str()).ok());
    auto host = b.build();

    auto plugin = lite_catalog::open(plugin_db.c_str());
    REQUIRE(plugin);

    catalog_registry reg;

    REQUIRE(reg.add("host", host));
    REQUIRE(reg.add("plugin", plugin));
    REQUIRE(!reg.add("host", host));
    REQUIRE(!reg.add("x", nullptr));
    REQUIRE(reg.size() == 2);

    REQUIRE(S(reg.tr(make_tag("zh-CN"), "Open")) == "打开");
    REQUIRE(S(reg.tr(make_tag("zh-CN"), "Plugin Only")) == "插件专用");
    REQUIRE(S(reg.get("plugin")->tr(make_tag("zh-CN"), "Open")) == "打开插件");
    REQUIRE(reg.get("none") == nullptr);
    REQUIRE(S(reg.tr(make_tag("zh-CN"), "Nothing")) == "Nothing");
    REQUIRE(S(reg.tr_n(make_tag("ja"), "a", "b", 2)) == "b");

    reg.set_default_language(make_tag("zh-CN"));
    REQUIRE(S(reg.tr("Plugin Only")) == "插件专用");
    REQUIRE(S(reg.tr_ctx("menu", "File")) == "文件菜单");

    auto tk = reg.remove("plugin");
    REQUIRE(!tk.released());
    REQUIRE(S(reg.tr(make_tag("zh-CN"), "Plugin Only")) == "Plugin Only");
    plugin.reset();
    REQUIRE(tk.wait_released(std::chrono::milliseconds(1000)));
    REQUIRE(fs::remove(plugin_db));

    REQUIRE(reg.remove("none").released());
    REQUIRE(release_token{}.released());

    {
        auto mine = std::make_shared<catalog_registry>();
        mine->add("host", host);
        mine->set_default_language(make_tag("ru"));
        set_global(mine);

        REQUIRE(S(mmbkpp::i18n::tr("Open")) == "Открыть");
        REQUIRE(S(mmbkpp::i18n::tr_n(make_tag("ru"), "%d file", "%d files", 5)) == "%d файлов");
        REQUIRE(global().get() == mine.get());

        set_global(nullptr);
        REQUIRE(global().get() != mine.get());
    }

    {
        catalog_registry reg2;
        reg2.add("host", host);
        reg2.set_default_language(make_tag("zh-CN"));  // 先设置为 zh-CN，避免初始 en 返回原文

        std::vector<std::thread> threads;
        std::atomic<int> bad{ 0 };

        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < 20000; ++j) {
                    const std::string r = S(reg2.tr("Open"));
                    if (r != "打开" && r != "Открыть")
                        ++bad;
                }
            });
        }

        std::thread switcher([&]() {
            for (int i = 0; i < 1000; ++i)
                reg2.set_default_language(make_tag(i % 2 == 0 ? "zh-CN" : "ru"));
        });

        for (auto& t : threads)
            t.join();
        switcher.join();

        REQUIRE(bad.load() == 0);
    }

    fs::remove(main_db);
}

// ============================================================================
// check_main on both modes
// ============================================================================

TEST_CASE("check_main fast", "[i18n]")
{
    const std::string main_db = make_db("main_check", MAIN_SQL);
    fast_catalog_builder b;
    REQUIRE(b.load_from_sqlite(main_db.c_str()).ok());
    auto fast = b.build();
    check_main(*fast);
    fs::remove(main_db);
}

TEST_CASE("check_main lite", "[i18n]")
{
    const std::string main_db = make_db("main_check_lite", MAIN_SQL);
    auto lite = lite_catalog::open(main_db.c_str());
    REQUIRE(lite);
    check_main(*lite);
    lite.reset();
    fs::remove(main_db);
}

// ============================================================================
// catalog state (3.3 #30-#35)
// ============================================================================

TEST_CASE("catalog state", "[i18n]")
{
    const std::string main_db = make_db("main_state", MAIN_SQL);
    fast_catalog_builder b;
    REQUIRE(b.load_from_sqlite(main_db.c_str()).ok());
    auto c = b.build();

    c->set_default_language(make_tag("ru"));
    REQUIRE(S(c->tr("Open")) == "Открыть");

    REQUIRE(S(c->tr_n("%d file", "%d files", 2)) == "%d файла");

    c->set_source_language(make_tag("zh-CN"));
    REQUIRE(S(c->tr(make_tag("zh-CN"), "Open")) == "Open");

    REQUIRE(S(c->tr(make_tag("zh-Hans-CN"), "Open")) == "Open");

    REQUIRE(S(c->tr(make_tag("ru"), "Open")) == "Открыть");

    REQUIRE(c->source_language() == make_tag("zh-CN"));
    REQUIRE(c->default_language() == make_tag("ru"));

    c.reset();
    fs::remove(main_db);
}
