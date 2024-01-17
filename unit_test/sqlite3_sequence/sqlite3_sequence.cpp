
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <mmbkpp/strg/sqlite3_sequence.hpp>

TEST_CASE("sqlite3_sequence - 01", "basic")
{
    mmbkpp::strg::sqlite3_sequence::global_init();
    
    do {
        auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
        seq->set_create_table_cb(
            [&](
                const mmbkpp::strg::sqlite3_hdl_sptr& _hdl, 
                const memepp::string& _table_name,
                mmbkpp::strg::sqlite3_sequence::index_id_t,
                mmbkpp::strg::sqlite3_sequence::node_id_t
                )
            {
                auto cmd = fmt::format("CREATE TABLE IF NOT EXISTS {} ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "name TEXT NOT NULL,"
                    "age INTEGER NOT NULL)",
                    _table_name);
                auto e = _hdl->do_write(cmd.data());
                REQUIRE(e.code() == 0);
        
            });
    
        do {

            auto rw_hdl_ret = seq->get_rw_hdl(0, 0);
            auto ro_hdl_ret = seq->get_ro_hdl(0, 0);

            REQUIRE(rw_hdl_ret.has_error() == false);
            REQUIRE(ro_hdl_ret.has_error() == false);
            REQUIRE(rw_hdl_ret.has_value() == true);
            REQUIRE(ro_hdl_ret.has_value() == true);
            REQUIRE(rw_hdl_ret.value() != nullptr);
            REQUIRE(ro_hdl_ret.value() != nullptr);

            REQUIRE(ghc::filesystem::is_directory(mm_to<memepp::native_string>(seq->dir_path())) == true);

            // check if the table is created
            auto cmd = fmt::format("SELECT name FROM sqlite_master WHERE type='table' AND name='{}'",
                seq->table_name());
            ro_hdl_ret.value()->do_read(cmd.data(),
                [&](int col_count, char** col_values, char** col_names)
            {
                REQUIRE(col_count == 1);
                REQUIRE(col_values[0] == seq->table_name());

                return 0;
            });

            REQUIRE(ghc::filesystem::is_regular_file(
                mm_to<memepp::native_string>(seq->filepath(0, 0))) == true);

            seq->try_close_idle_hdl();
            seq->try_clean_dir_by_removing_out_of_range(0);
            
            REQUIRE(ghc::filesystem::exists(
                mm_to<memepp::native_string>(seq->filepath(0, 0))) == true);
        } while (0);

        seq->try_close_idle_hdl();
        seq->try_clean_dir_by_removing_out_of_range(0);
        
        REQUIRE(ghc::filesystem::exists(
            mm_to<memepp::native_string>(seq->filepath(0, 0))) == false);

        do {
            auto rw_hdl_ret_1_0 = seq->get_rw_hdl(1, 0);
            auto ro_hdl_ret_0_0 = seq->get_ro_hdl(0, 0);

            REQUIRE(rw_hdl_ret_1_0.has_error() == false);
            REQUIRE(ro_hdl_ret_0_0.has_error() == true );
            REQUIRE(rw_hdl_ret_1_0.has_value() == true );
            REQUIRE(ro_hdl_ret_0_0.has_value() == false);

            auto rw_hdl_ret_2_0 = seq->get_rw_hdl(2, 0);
            auto ro_hdl_ret_2_0 = seq->get_ro_hdl(2, 0);

            REQUIRE(rw_hdl_ret_2_0.has_error() == false);
            REQUIRE(ro_hdl_ret_2_0.has_error() == false);
            REQUIRE(rw_hdl_ret_2_0.has_value() == true );
            REQUIRE(ro_hdl_ret_2_0.has_value() == true );
            
            REQUIRE(ghc::filesystem::exists(
                mm_to<memepp::native_string>(seq->filepath(0, 0))) == false);
            REQUIRE(ghc::filesystem::exists(
                mm_to<memepp::native_string>(seq->filepath(1, 0))) == true );
            REQUIRE(ghc::filesystem::exists(
                mm_to<memepp::native_string>(seq->filepath(2, 0))) == true );
        } while (0);
        seq->set_max_kb(20);
        seq->try_clean_dir_to_limit();
        
        REQUIRE(ghc::filesystem::exists(
            mm_to<memepp::native_string>(seq->filepath(1, 0))) == false);
        REQUIRE(ghc::filesystem::exists(
            mm_to<memepp::native_string>(seq->filepath(2, 0))) == true );
    } while (0);
    
}
