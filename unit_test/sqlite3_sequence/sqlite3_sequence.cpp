
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <mmbkpp/strg/sqlite3_sequence.hpp>

TEST_CASE("sqlite3_sequence - 01", "basic")
{
    mmbkpp::strg::sqlite3_sequence::global_init();
    
    do {
        auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
        seq->set_open_after_create_table_cb(
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

            auto rw_hdl_ret_i0_n0_0 = seq->get_rw_hdl(0, 0);
            auto ro_hdl_ret_i0_n0_0 = seq->get_ro_hdl(0, 0);

            auto rw_hdl = rw_hdl_ret_i0_n0_0.value();
            auto ro_hdl = ro_hdl_ret_i0_n0_0.value();

            REQUIRE(rw_hdl_ret_i0_n0_0.has_error() == false);
            REQUIRE(ro_hdl_ret_i0_n0_0.has_error() == false);
            REQUIRE(rw_hdl_ret_i0_n0_0.has_value() == true);
            REQUIRE(ro_hdl_ret_i0_n0_0.has_value() == true);
            REQUIRE(rw_hdl != nullptr);
            REQUIRE(ro_hdl != nullptr);

            REQUIRE(ghc::filesystem::is_directory(mm_to<memepp::native_string>(seq->dir_path())) == true);

            // check if the table is created
            auto cmd = fmt::format("SELECT name FROM sqlite_master WHERE type='table' AND name='{}'",
                seq->table_name());
            ro_hdl->do_read(cmd.data(),
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

            auto rw_hdl_ret_i0_n0_1 = seq->get_rw_hdl(0, 0);

            REQUIRE(rw_hdl_ret_i0_n0_1.has_error() == true);
        } while (0);

        seq->try_close_idle_hdl();
        seq->try_clean_dir_by_removing_out_of_range(0);
        
        REQUIRE(ghc::filesystem::exists(
            mm_to<memepp::native_string>(seq->filepath(0, 0))) == false);

        do {
            auto rw_hdl_ret_i1_n0 = seq->get_rw_hdl(1, 0);
            auto ro_hdl_ret_i0_n0 = seq->get_ro_hdl(0, 0);

            auto rw_hdl_i1_n0 = rw_hdl_ret_i1_n0.value();
            //auto ro_hdl_0_0 = ro_hdl_ret_0_0.value().lock();

            REQUIRE(rw_hdl_ret_i1_n0.has_error() == false);
            REQUIRE(ro_hdl_ret_i0_n0.has_error() == true );
            REQUIRE(rw_hdl_ret_i1_n0.has_value() == true );
            REQUIRE(ro_hdl_ret_i0_n0.has_value() == false);

            auto rw_hdl_ret_i2_n0 = seq->get_rw_hdl(2, 0);
            auto ro_hdl_ret_i2_n0 = seq->get_ro_hdl(2, 0);

            auto rw_hdl_i2_n0 = rw_hdl_ret_i2_n0.value();
            auto ro_hdl_i2_n0 = ro_hdl_ret_i2_n0.value();

            REQUIRE(rw_hdl_ret_i2_n0.has_error() == false);
            REQUIRE(ro_hdl_ret_i2_n0.has_error() == false);
            REQUIRE(rw_hdl_ret_i2_n0.has_value() == true );
            REQUIRE(ro_hdl_ret_i2_n0.has_value() == true );
            
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

        auto db_filepath_2_0 = mm_to<memepp::native_string>(seq->filepath(2, 0));
        REQUIRE(ghc::filesystem::exists(db_filepath_2_0) == true);

        seq->set_dir_path(mmupp::fs::relative_with_program_path("new_seqs"),
            mmbkpp::strg::sqlite3_sequence::old_action_t::move_old);
        
        REQUIRE(ghc::filesystem::exists(db_filepath_2_0) == false);
        REQUIRE(ghc::filesystem::exists(
            mm_to<memepp::native_string>(seq->filepath(2, 0))) == true);
        
        ghc::filesystem::remove(
            mm_to<memepp::native_string>(seq->filepath(2, 0)));
    } while (0);
    
}

TEST_CASE("sqlite3_sequence - 02", "mutli threads")
{
    if (false)
    { }
    else {
        srand(time(0));

        auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
        seq->set_dir_path(mmupp::fs::relative_with_program_path("mt_db_seqs"));
        seq->set_open_after_create_table_cb(
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
        
        bool thd_exit = false;
        std::vector<std::thread> rw_thds;
        for (int i = 0; i < 16; ++i) 
        {
            rw_thds.emplace_back([&]()
            {
                while (!thd_exit)
                {
                    auto rw_hdl_ret_i0_n0 = 
                        seq->get_rw_hdl_wait_for(0, 0, std::chrono::seconds(1));

                    auto rw_hdl_i0_n0 = rw_hdl_ret_i0_n0.value();

                    auto write_err_i0_n0 = 
                        rw_hdl_i0_n0->do_write_wait_for(fmt::format(
                        "INSERT INTO {} (name, age) VALUES ('{}', {})",
                        seq->table_name(),
                        fmt::format("name_{:0>16}", rand()),
                        rand()
                    ).data(), std::chrono::seconds(10));
                    
                    REQUIRE(write_err_i0_n0.code() == 0);
                    
                    REQUIRE(rw_hdl_ret_i0_n0.has_error() == false);
                    //REQUIRE(rw_hdl_ret_i0_n0.has_value() == true );
                }
            });
        }

        std::vector<std::thread> ro_thds;
        for (int i = 0; i < 16; ++i)
        {
            ro_thds.emplace_back([&]()
            {
                while (!thd_exit)
                {
                    auto ro_hdl_ret_i0_n0 = 
                        seq->get_ro_hdl_wait_for(0, 0, std::chrono::seconds(1));
                    
                    auto ro_hdl_i0_n0 = ro_hdl_ret_i0_n0.value();
                    
                    auto read_err_i0_n0 = 
                        ro_hdl_i0_n0->do_read_wait_for(fmt::format(
                        "SELECT * FROM {}",
                        seq->table_name()
                    ).data(), std::chrono::seconds(10),
                        [&](int _argc, char** _argv, char** _col_name)
                    {
                        return 0;
                    });

                    REQUIRE(read_err_i0_n0.code() == 0);

                    REQUIRE(ro_hdl_ret_i0_n0.has_error() == false);
                    //REQUIRE(ro_hdl_ret_i0_n0.has_value() == true );
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
        //std::this_thread::sleep_for(std::chrono::minutes(10));

        thd_exit = true;
        for (auto& thd : rw_thds)
        {
            thd.join();
        }

        for (auto& thd : ro_thds)
        {
            thd.join();
        }
    }
}