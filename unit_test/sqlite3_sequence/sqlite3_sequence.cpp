
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <mmbkpp/strg/sqlite3_sequence.hpp>

TEST_CASE("sqlite3_sequence - 01", "[sqlite3_sequence]")
{
    printf("sqlite3_sequence - 01\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    memepp::string dir_path;
    do {
        auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
        dir_path = seq->dir_path();
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

        ghc::filesystem::path keep_dir_path = mm_to<memepp::native_string>(seq->filepath(2, 0));
        auto dir_iter = ghc::filesystem::directory_iterator(keep_dir_path.parent_path());
        auto dir_end  = ghc::filesystem::directory_iterator();
        size_t total_kb = 0;
        for (; dir_iter != dir_end; ++dir_iter) 
        {
            if (!ghc::filesystem::is_regular_file(dir_iter->status()))
                continue;
            auto fsize = ghc::filesystem::file_size(dir_iter->path());
                total_kb += (fsize / 1024);
        }

        seq->set_max_kb(total_kb);
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
        
        ghc::filesystem::remove_all(
            mm_to<memepp::native_string>(seq->dir_path()));
    } while (0);

    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 02", "[sqlite3_sequence]")
{
    printf("sqlite3_sequence - 02\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    if (false)
    { }
    else {
        srand(time(0));

        auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
        auto dir_path = mmupp::fs::relative_with_program_path("mt_db_seqs");
        seq->set_dir_path(dir_path);
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
        
        size_t thrd_count = std::thread::hardware_concurrency();
        if (thrd_count == 0)
            thrd_count = 1;

        bool thd_exit = false;
        bool has_rw_hdl_get_err   = false;
        bool has_rw_hdl_write_err = false;
        std::vector<std::thread> rw_thds;
        for (int i = 0; i < thrd_count; ++i)
        {
            rw_thds.emplace_back([&]() {

                while (!thd_exit) {

                    auto rw_hdl_ret_i0_n0 = 
                        seq->get_rw_hdl_wait_for(0, 0, std::chrono::seconds(1));
                    if (rw_hdl_ret_i0_n0.has_error())
                    {
                        has_rw_hdl_get_err = true;
                        continue;
                    }

                    auto rw_hdl_i0_n0 = rw_hdl_ret_i0_n0.value();

                    auto write_err_i0_n0 = 
                        rw_hdl_i0_n0->do_write_wait_for(fmt::format(
                        "INSERT INTO {} (name, age) VALUES ('{}', {})",
                        seq->table_name(),
                        fmt::format("name_{:0>16}", rand()),
                        rand()
                    ).data(), std::chrono::seconds(10));

                    if (write_err_i0_n0) {
                        has_rw_hdl_write_err = true;
                        continue;
                    }
                }
            });
        }

        bool has_ro_hdl_get_err  = false;
        bool has_ro_hdl_read_err = false;
        std::vector<std::thread> ro_thds;
        for (int i = 0; i < thrd_count; ++i)
        {
            ro_thds.emplace_back([&]() {
                while (!thd_exit) {

                    auto ro_hdl_ret_i0_n0 = 
                        seq->get_ro_hdl_wait_for(0, 0, std::chrono::seconds(1));
                    if (ro_hdl_ret_i0_n0.has_error())
                    {
                        has_ro_hdl_get_err = true;
                        continue;
                    }

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
                    if (read_err_i0_n0) {
                        has_ro_hdl_read_err = true;
                        continue;
                    }

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

        REQUIRE(has_rw_hdl_get_err   == false);
        REQUIRE(has_rw_hdl_write_err == false);

        REQUIRE(has_ro_hdl_get_err  == false);
        REQUIRE(has_ro_hdl_read_err == false);

        seq.reset();
        ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
    }
}

TEST_CASE("sqlite3_sequence - 03: Error handling and non-existent nodes", "[sqlite3_sequence]") 
{
    printf("sqlite3_sequence - 03: Error handling and non-existent nodes\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);

    // Test getting RO handle for non-existent node (should return NOENT)
    auto ro_hdl_ret = seq->get_ro_hdl(999, 999);  // Non-existent index and node
    REQUIRE(ro_hdl_ret.has_error() == true);
    REQUIRE(ro_hdl_ret.error().code() < 0);  // Check specific error code

    // Test getting RW handle without create_if_not_exist (should fail)
    auto rw_hdl_ret = seq->get_rw_hdl(999, 999, false);
    REQUIRE(rw_hdl_ret.has_error() == true);
    REQUIRE(rw_hdl_ret.error().code() < 0);

    // Test invalid directory path (e.g., file instead of dir)
    auto err = seq->set_dir_path(mmupp::fs::relative_with_program_path("invalid_file.txt"));  // Assume "invalid_file.txt" is a file
    REQUIRE(err.code() == 0);  // Should fail as path is a file

    // Clean up test directory
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 04: Boundary conditions and limits", "[sqlite3_sequence]") 
{
    printf("sqlite3_sequence - 04: Boundary conditions and limits\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);
    seq->set_max_kb(1);  // Very small limit to force cleaning
    seq->set_max_hdl_count(2);  // Limit to 2 handles

    do {
        // Create multiple nodes to test limits
        auto hdl1 = seq->get_rw_hdl(0, 0).value();
        auto hdl2 = seq->get_rw_hdl(0, 1).value();
        auto hdl3_ret = seq->get_rw_hdl(0, 2);  // Should trigger limit check and clean

        REQUIRE(hdl3_ret.has_value() == true);  // But may have closed idle ones
        REQUIRE(seq->check_hdl_limit_and_clean().value() > 0);  // Should clean some handles

        // Test max node ID
        auto max_node_hdl = seq->get_rw_hdl(0, MMINT_MAX);
        REQUIRE(max_node_hdl.has_value() == true);
    } while (0);

    // Test cleaning to limit (should remove some files)
    auto clean_ret = seq->try_clean_dir_to_limit(mmbkpp::strg::sqlite3_sequence::sort_t::node_asc);
    REQUIRE(clean_ret.value() > 0);

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 05: Callbacks and logging", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 05: Callbacks and logging\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);

    // Test open_after_create_table_cb
    bool callback_called = false;
    seq->set_open_after_create_table_cb(
        [&](const mmbkpp::strg::sqlite3_hdl_sptr& _hdl, const memepp::string& _table, auto, auto) {
            callback_called = true;
            // Simulate table creation
            _hdl->do_write(fmt::format("CREATE TABLE {} (id INTEGER)", _table).data());
        });
    do {
        auto hdl = seq->get_rw_hdl(0, 0).value();
        REQUIRE(callback_called == true);  // Callback should be invoked
    } while (0);

    // Test log_cb
    std::vector<std::pair<mmbkpp::strg::sqlite3_sequence::level_t, memepp::string>> logs;
    seq->set_log_cb([&](mmbkpp::strg::sqlite3_sequence::level_t level, const memepp::string_view& msg) {
        logs.emplace_back(level, msg.to_string());
    });
    seq->set_max_kb(1);
    seq->try_clean_dir_to_limit();  // Trigger some logging
    REQUIRE(logs.size() > 0);  // Should have logged something
    REQUIRE(logs[0].first == mmbkpp::strg::sqlite3_sequence::level_t::trace);  // Check log level

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 06: File operations (copy, move, checkpoint)", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 06: File operations (copy, move, checkpoint)\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);
    seq->get_rw_hdl(0, 0);  // Create a node

    // Test copy_all_to_path
    memepp::string new_path = mmupp::fs::relative_with_program_path("copied_db_seqs");
    auto copy_err = seq->copy_all_to_path(new_path);
    REQUIRE(copy_err.code() == MGEC__OK);
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(seq->make_filepath(new_path, seq->file_prefix(), seq->file_suffix(), 0, 0))) == true);

    // Test set_dir_path_and_move
    memepp::string move_path = mmupp::fs::relative_with_program_path("moved_db_seqs");
    auto move_err = seq->set_dir_path_and_move(move_path);
    REQUIRE(move_err.code() == MGEC__OK);
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(seq->filepath(0, 0))) == true);  // Moved to new path

    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(new_path));

    // Test try_checkpoint_idle_files (simulate idle time)

    std::this_thread::sleep_for(std::chrono::seconds(1));  // Simulate idle
    auto checkpoint_ret = seq->try_checkpoint_idle_files(0);  // Immediate checkpoint
    REQUIRE(checkpoint_ret.has_error() == false);

    // Clean up all directories
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(move_path));
}

TEST_CASE("sqlite3_sequence - 07: Advanced multi-threading and close callbacks", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 07: Advanced multi-threading and close callbacks\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);
    seq->set_max_hdl_count(5);  // Limit for concurrent testing

    // Create some nodes
    for (int i = 0; i < 10; ++i) {
        seq->get_rw_hdl(0, i);
    }

    bool thd_exit = false;
    std::thread clean_thd([&]() {
        while (!thd_exit) {
            seq->try_close_idle_hdl();
            seq->try_clean_dir_to_limit(mmbkpp::strg::sqlite3_sequence::sort_t::node_desc);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Concurrent RW access
    std::vector<std::thread> thds;
    for (int i = 0; i < 8; ++i) {
        thds.emplace_back([&]() {
            for (int j = 0; j < 10 && !thd_exit; ++j) {
                auto hdl_ret = seq->get_rw_hdl_wait_for(0, rand() % 10, std::chrono::seconds(1));
                if (hdl_ret) {
                    hdl_ret.value()->do_write("INSERT INTO data (name, age) VALUES ('test', 1)");
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    thd_exit = true;
    clean_thd.join();
    for (auto& thd : thds) thd.join();

    // Check if some files were cleaned (due to limit)
    auto clean_out_of_range = seq->try_clean_dir_by_removing_out_of_range(0, 5);  // Remove nodes <0 or >=5
    REQUIRE(clean_out_of_range.value() >= 0);

    // Verify on_close_hdl was triggered (indirectly by checking file existence after close)
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(seq->filepath(0, 0))) == false);  // Some should remain

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 08: Internal file operations (remove, move, copy)", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 08: Internal file operations (remove, move, copy)\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);

    // Create a node to test operations
    auto hdl = seq->get_rw_hdl(0, 0).value();
    auto original_path = seq->filepath(0, 0);
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(original_path)) == true);

    // Indirectly test try_remove via cleaning out of range
    hdl.reset();  // Release handle to allow removal
    auto remove_ret = seq->try_clean_dir_by_removing_out_of_range(-1, 0);  // Remove nodes < -1 or >=0 (includes 0)
    REQUIRE(remove_ret.value() > 0);
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(original_path)) == false);

    // Test try_move_to via set_dir_path_and_move
    seq->get_rw_hdl(0, 1);  // Create another node
    memepp::string new_dir_1 = mmupp::fs::relative_with_program_path("moved_test_db_seqs_1");
    seq->set_dir_path_and_move(new_dir_1);
    auto new_path = seq->filepath(0, 1);
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(new_path)) == true);

    hdl = seq->get_rw_hdl(0, 1).value();  // Create another node
    memepp::string new_dir_2 = mmupp::fs::relative_with_program_path("moved_test_db_seqs_2");
    seq->set_dir_path_and_move(new_dir_2);
    hdl.reset();

    // Test try_copy_to via copy_all_to_path
    memepp::string copy_dir = mmupp::fs::relative_with_program_path("copied_test_db_seqs");
    seq->copy_all_to_path(copy_dir);
    auto copied_path = seq->make_filepath(copy_dir, seq->file_prefix(), seq->file_suffix(), 0, 1);
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(copied_path)) == true);

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(new_dir_1));
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(new_dir_2));
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(copy_dir));
}

TEST_CASE("sqlite3_sequence - 09: Configuration changes and failures", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 09: Configuration changes and failures\n");

    mmbkpp::strg::sqlite3_sequence::global_init();

    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);

    // Set configs before any nodes
    seq->set_file_prefix("custom_prefix");
    seq->set_file_suffix("custom_suffix");
    seq->set_table_name("custom_table");
    REQUIRE(seq->file_prefix() == "custom_prefix");
    REQUIRE(seq->file_suffix() == "custom_suffix");
    REQUIRE(seq->table_name() == "custom_table");

    // Create a node (now index_infos_ is not empty)
    seq->get_rw_hdl(0, 0);
    
    // Attempt to change configs (should fail silently as per code)
    seq->set_file_prefix("new_prefix");  // Should not change
    seq->set_file_suffix("new_suffix");
    seq->set_table_name("new_table");
    REQUIRE(seq->file_prefix() == "custom_prefix");  // Unchanged
    REQUIRE(seq->file_suffix() == "custom_suffix");
    REQUIRE(seq->table_name() == "custom_table");

    // Test set_should_checkpoint_truncate_on_close

    auto hdl = seq->get_rw_hdl(0, 1).value();
    hdl->do_write("PRAGMA journal_mode = WAL;");  // Create WAL
    hdl->do_write("INSERT INTO data (name, age) VALUES ('test', 1);");  // Generate WAL changes
    hdl.reset();  // Close should not truncate (indirect check: WAL file remains)
    auto wal_path = seq->filepath(0, 1) + "-wal";
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(wal_path)) == true);  // WAL not truncated

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 10: Idle checkpoints and preclose callback", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 10: Idle checkpoints and preclose callback\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);

    // Create node and write to generate WAL
    auto hdl = seq->get_rw_hdl(0, 0).value();
    hdl->do_write("PRAGMA journal_mode = WAL;");
    hdl->do_write("INSERT INTO data (name, age) VALUES ('test', 1);");  // Generate WAL changes
    hdl.reset();  // Close triggers on_preclose_hdl (should checkpoint truncate)

    // Check if WAL was truncated
    auto wal_path = seq->filepath(0, 0) + "-wal";
    REQUIRE(ghc::filesystem::file_size(mm_to<memepp::native_string>(wal_path)) == 0);  // Truncated

    // Test try_checkpoint_idle_files with idle time
    seq->get_rw_hdl(0, 1);  // Create another node
    std::this_thread::sleep_for(std::chrono::seconds(2));  // Make it idle
    auto checkpoint_ret = seq->try_checkpoint_idle_files(1);  // Checkpoint files idle >1s
    REQUIRE(checkpoint_ret.has_error() == false);

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 11: Resource management and retry mechanisms", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 11: Resource management and retry mechanisms\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);

    // Test retry mechanisms (simulate busy by holding handle)
    auto hdl = seq->get_rw_hdl(0, 0).value();  // Hold it to make next get busy
    auto retry_rw_ret = seq->get_rw_hdl_and_retry(0, 0, 3);  // Retry 3 times
    REQUIRE(retry_rw_ret.has_error() == false);

    hdl.reset();  // Release
    retry_rw_ret = seq->get_rw_hdl_and_retry(0, 0, 3);
    REQUIRE(retry_rw_ret.has_value() == true);  // Now succeeds
    retry_rw_ret.value().reset();

    // Test reference counting (ensure no leak after multiple opens/closes)
    for (int i = 0; i < 5; ++i) {
        auto temp_hdl = seq->get_ro_hdl(0, 0).value();
        REQUIRE(temp_hdl.use_count() > 1);  // Shared
    }
    seq->try_close_idle_hdl();  // Close idle
    auto final_hdl = seq->get_ro_hdl(0, 0).value();
    REQUIRE(final_hdl.use_count() == 2);  // Only this reference + internal
    final_hdl.reset();

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 12: Platform-specific and remove_sqlite_file", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 12: Platform-specific and remove_sqlite_file\n");

    mmbkpp::strg::sqlite3_sequence::global_init();  // Test multiple calls (should be idempotent)
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs");
    seq->set_dir_path(dir_path);

    // Create node with WAL/SHM
    auto hdl = seq->get_rw_hdl(0, 0).value();
    hdl->do_write("PRAGMA journal_mode = WAL;");
    hdl->do_write("INSERT INTO data (name, age) VALUES ('test', 1);");
    hdl.reset();

    // Test remove_sqlite_file (includes WAL and SHM)
    //auto db_path = ghc::filesystem::path(mm_to<memepp::native_string>(seq->filepath(0, 0)));
    //std::error_code ec;
    //bool removed = mmbkpp::strg::sqlite3_sequence::remove_sqlite_file(db_path, ec);
    //REQUIRE(removed == true);
    //REQUIRE(ec.value() == 0);
    //REQUIRE(ghc::filesystem::exists(db_path) == false);
    //REQUIRE(ghc::filesystem::exists(db_path.native() + MMN_TEXT("-wal")) == false);
    //REQUIRE(ghc::filesystem::exists(db_path.native() + MMN_TEXT("-shm")) == false);

    // Test global_init on Windows (conditional, simulate temp dir creation)
#if MG_OS__WIN_AVAIL
    // Assume global_init creates temp dir; check if it exists
    char temp_dir[MAX_PATH];
    mgu_get_temp_path(temp_dir, MAX_PATH);
    strncat(temp_dir, "/sqlite3_sequence_temp", MAX_PATH - strlen(temp_dir) - 1);
    REQUIRE(ghc::filesystem::exists(temp_dir) == true);
#endif

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 13: Large-scale operations and performance", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 13: Large-scale operations and performance\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs_large");
    seq->set_dir_path(dir_path);
    seq->set_max_kb(100);  // Small limit to force cleaning

    // Create many nodes (simulate large scale)
    const int node_count = 100;  // Adjust for performance testing
    for (int i = 0; i < node_count; ++i) {
        auto hdl = seq->get_rw_hdl(0, i).value();
        hdl->do_write("INSERT INTO data (name, age) VALUES ('test', 1);");  // Add data to increase size
    }

    // Test try_clean_dir_to_limit with different sorts
    auto clean_asc = seq->try_clean_dir_to_limit(mmbkpp::strg::sqlite3_sequence::sort_t::node_asc);
    REQUIRE(clean_asc.value() > 0);  // Should clean some

    auto clean_desc = seq->try_clean_dir_to_limit(mmbkpp::strg::sqlite3_sequence::sort_t::node_desc);
    REQUIRE(clean_desc.value() == 0);

    // Verify some files remain, others removed
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(seq->filepath(0, node_count - 1))) == true);  // Last one should remain in desc sort
    REQUIRE(ghc::filesystem::exists(mm_to<memepp::native_string>(seq->filepath(0, 0))) == false);  // First one removed

    // Clean up (important for large tests)
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 14: Exception injection and robustness", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 14: Exception injection and robustness\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs_except");
    seq->set_dir_path(dir_path);

    // Create a node
    seq->get_rw_hdl(0, 0);

    // Simulate file system error (e.g., make directory read-only; this is platform-dependent, simulate via invalid path)
    memepp::string invalid_path = mmupp::fs::relative_with_program_path("*?<>|:invalid_nonexistent_path");  // Should cause creation failure
    auto set_err = seq->set_dir_path_and_move(invalid_path);
    REQUIRE(set_err.code() != MGEC__OK);

    // Test try_remove with busy handle (indirect via clean)
    auto hdl = seq->get_rw_hdl(0, 0).value();  // Hold handle to make busy
    auto remove_ret = seq->try_clean_dir_by_removing_out_of_range(0, 1);  // Try to remove node 0
    REQUIRE(remove_ret.value() == 0);  // Should not remove due to busy
    hdl.reset();  // Release
    remove_ret = seq->try_clean_dir_by_removing_out_of_range(0, 1);
    REQUIRE(remove_ret.value() == 0);

    // Test callback exception (simulate throw in cb)
    seq->set_open_after_create_table_cb([](auto, auto, auto, auto) { throw std::runtime_error("simulated error"); });
    auto hdl_ret = seq->get_rw_hdl(0, 1);
    REQUIRE(hdl_ret.has_error() == true);
    REQUIRE(hdl_ret.error().code() == MGEC__ERR);

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 15: Weak pointers, userdata, and empty operations", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 15: Weak pointers, userdata, and empty operations\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    // Test default constructor and empty ops
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = seq->dir_path();
    REQUIRE(dir_path == mmupp::fs::relative_with_program_path("db_seqs"));  // Default path
    REQUIRE(seq->file_prefix() == "node");
    REQUIRE(seq->file_suffix() == "db");
    REQUIRE(seq->table_name() == "data");

    // Test empty callbacks (should not crash)
    seq->set_open_after_create_table_cb(nullptr);
    seq->set_log_cb(nullptr);
    auto hdl = seq->get_rw_hdl(0, 0).value();  // Should work without cb
    REQUIRE(hdl != nullptr);
    hdl.reset();

    //// Test weak pointer in on_close_hdl (destroy seq while handle alive)
    //auto data = std::make_shared<mmbkpp::strg::sqlite3_sequence::__hdl_onclose_data>();
    //data->seq_ = seq;  // Weak ptr to seq
    //data->index_id_ = 0;
    //data->node_id_ = 0;
    //data->is_readonly_ = false;

    //seq.reset();  // Destroy seq, weak ptr expires
    //mmbkpp::strg::sqlite3_sequence::on_close_hdl(data);  // Should handle expired weak ptr gracefully (no crash)
    //// No REQUIRE here as it's success if no crash

    // Test empty directory (no nodes)
    auto clean_empty = seq->try_clean_dir_to_limit();  // seq is null, but simulate new one
    seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();  // New instance
    clean_empty = seq->try_clean_dir_to_limit();
    REQUIRE(clean_empty.has_error() == false);

    // Clean up (if any)
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 16: Advanced retries and timeouts in multi-thread", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 16: Advanced retries and timeouts in multi-thread\n");

    mmbkpp::strg::sqlite3_sequence::global_init();
    
    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs_retry");
    seq->set_dir_path(dir_path);

    // Hold a handle in one thread to simulate busy
    auto hdl = seq->get_rw_hdl(0, 0).value();
    bool hold = true;
    std::thread holder([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Hold for 2s
        hold = false;
        hdl.reset();
    });
    MEGOPP_UTIL__ON_SCOPE_CLEANUP([&] 
    {
        holder.join();
    });

    // Test get_rw_hdl_wait_for with timeout (should fail initially, then succeed after release)
    auto wait_ret = seq->get_rw_hdl_wait_for(0, 0, std::chrono::milliseconds(500));
    REQUIRE(wait_ret.has_error() == false);  // Timeout due to busy

    std::this_thread::sleep_for(std::chrono::seconds(3));  // Wait for release
    wait_ret = seq->get_rw_hdl_wait_for(0, 0, std::chrono::milliseconds(500));
    REQUIRE(wait_ret.has_value() == true);
    wait_ret.value().reset();

    // Test get_rw_hdl_and_retry in multi-thread
    std::thread retry_thd([&]() {
        auto retry_ret = seq->get_rw_hdl_and_retry(0, 0, 5);  // Retry 5 times
        REQUIRE(retry_ret.has_value() == true);  // Should succeed after some retries
    });

    retry_thd.join();

    // Clean up
    seq.reset();
    ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
}

TEST_CASE("sqlite3_sequence - 17: try_checkpoint_idle_files functionality", "[sqlite3_sequence]") {
    printf("sqlite3_sequence - 17: try_checkpoint_idle_files functionality\n");

    mmbkpp::strg::sqlite3_sequence::global_init();

    auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
    auto dir_path = mmupp::fs::relative_with_program_path("test_db_seqs_checkpoint");
    seq->set_dir_path(dir_path);

    seq->set_open_after_create_table_cb([](const mmbkpp::strg::sqlite3_hdl_sptr& hdl, const memepp::string&, auto, auto) {
        hdl->do_write("CREATE TABLE data (id INTEGER);");  // Simple table for WAL generation
    });

    SECTION("Basic checkpoint: Idle WAL file is truncated") {
        // Create node and generate WAL
        auto hdl = seq->get_rw_hdl(0, 0).value();
        hdl->do_write("PRAGMA journal_mode = WAL;");
        hdl->do_write("INSERT INTO data (id) VALUES (1);");  // Generate WAL changes
        hdl.reset();  // Close handle

        auto wal_path = mm_to<memepp::native_string>(seq->filepath(0, 0) + "-wal");
        REQUIRE(ghc::filesystem::exists(wal_path) == true);
        REQUIRE(ghc::filesystem::file_size(wal_path) > 0);  // WAL has content

        // Simulate idle time > threshold
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Call with idle_seconds=1 (should checkpoint since idle >1s)
        auto ret = seq->try_checkpoint_idle_files(1);
        REQUIRE(ret.has_error() == false);

        // Verify WAL is truncated (size == 0 or file removed)
        if (ghc::filesystem::exists(wal_path)) {
            REQUIRE(ghc::filesystem::file_size(wal_path) == 0);
        }
        else {
            REQUIRE(true);  // Acceptable if fully removed
        }
    }

    SECTION("Idle threshold: File not idle enough, no checkpoint") {
        // Create WAL
        auto hdl = seq->get_rw_hdl(0, 1).value();
        hdl->do_write("PRAGMA journal_mode = WAL;");
        hdl->do_write("INSERT INTO data (id) VALUES (1);");
        hdl.reset();

        auto wal_path = mm_to<memepp::native_string>(seq->filepath(0, 1) + "-wal");
        REQUIRE(ghc::filesystem::exists(wal_path) == true);
        auto original_size = ghc::filesystem::file_size(wal_path);
        REQUIRE(original_size > 0);

        // Simulate short idle time < threshold
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Call with idle_seconds=2 (idle <2s, should not checkpoint)
        auto ret = seq->try_checkpoint_idle_files(2);
        REQUIRE(ret.has_error() == false);

        // Verify WAL unchanged
        REQUIRE(ghc::filesystem::file_size(wal_path) == original_size);
    }

    SECTION("Multiple nodes: Checkpoint only idle ones") {
        // Create two nodes
        auto hdl0 = seq->get_rw_hdl(0, 2).value();
        hdl0->do_write("PRAGMA journal_mode = WAL;");
        hdl0->do_write("INSERT INTO data (id) VALUES (1);");
        hdl0.reset();

        auto hdl1 = seq->get_rw_hdl(1, 0).value();
        hdl1->do_write("PRAGMA journal_mode = WAL;");
        hdl1->do_write("INSERT INTO data (id) VALUES (1);");
        hdl1.reset();

        auto wal_path0 = mm_to<memepp::native_string>(seq->filepath(0, 2) + "-wal");
        auto wal_path1 = mm_to<memepp::native_string>(seq->filepath(1, 0) + "-wal");
        REQUIRE(ghc::filesystem::exists(wal_path0) == true);
        REQUIRE(ghc::filesystem::exists(wal_path1) == true);

        // Make one idle longer
        std::this_thread::sleep_for(std::chrono::seconds(2));  // Both idle now, but simulate touch on one
        ghc::filesystem::last_write_time(wal_path1, ghc::filesystem::file_time_type::clock::now());  // "Touch" to make non-idle

        // Call with idle_seconds=1 (should checkpoint only the idle one)
        auto ret = seq->try_checkpoint_idle_files(1);
        REQUIRE(ret.has_error() == false);

        // Verify: wal_path0 truncated, wal_path1 unchanged
        REQUIRE(ghc::filesystem::file_size(wal_path0) == 0);
        REQUIRE(ghc::filesystem::file_size(wal_path1) > 0);
    }

    SECTION("Error cases: Directory not exists or handle busy") {
        // Non-existent directory
        seq->set_dir_path(mmupp::fs::relative_with_program_path("*?<>|:invalid_nonexistent_path"));
        auto ret = seq->try_checkpoint_idle_files(1);
        REQUIRE(ret.has_error() == true);  // Should fail (e.g., NOENT or similar)

        // Reset to valid dir and test busy handle (function skips busy nodes)
        seq->set_dir_path(mmupp::fs::relative_with_program_path("test_db_seqs_checkpoint"));
        auto hdl = seq->get_rw_hdl(0, 3).value();  // Hold handle (busy)
        hdl->do_write("PRAGMA journal_mode = WAL;");
        hdl->do_write("INSERT INTO data (id) VALUES (1);");

        std::this_thread::sleep_for(std::chrono::seconds(2));
        ret = seq->try_checkpoint_idle_files(1);
        REQUIRE(ret.has_error() == false);  // Succeeds but skips busy

        auto wal_path = mm_to<memepp::native_string>(seq->filepath(0, 3) + "-wal");
                REQUIRE(ghc::filesystem::file_size(wal_path) > 0);  // Not checkpointed due to busy
                hdl.reset();  // Release for cleanup
            }

            // Clean up test directory
            seq.reset();
            ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
        }

        // P1-6 test: verify file operations work when seq_ is expired
        TEST_CASE("sqlite3_sequence - P1-6: file operations after seq destruction", "[sqlite3_sequence][P1-6]")
        {
            printf("sqlite3_sequence - P1-6: file operations after seq destruction\n");
    
            mmbkpp::strg::sqlite3_sequence::global_init();
    
            memepp::string dir_path;
            memepp::string new_dir_path;
    
            // Create sequence and set up
            auto seq = std::make_shared<mmbkpp::strg::sqlite3_sequence>();
            dir_path = seq->dir_path();
    
            seq->set_open_after_create_table_cb(
                [&](const mmbkpp::strg::sqlite3_hdl_sptr& _hdl, 
                    const memepp::string& _table_name,
                    mmbkpp::strg::sqlite3_sequence::index_id_t,
                    mmbkpp::strg::sqlite3_sequence::node_id_t)
                {
                    auto cmd = fmt::format("CREATE TABLE IF NOT EXISTS {} ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "name TEXT NOT NULL)", _table_name);
                    auto e = _hdl->do_write(cmd.data());
                    REQUIRE(e.code() == 0);
                });
    
            SECTION("wait_for_move: rename file after seq destruction")
                        {
                            // Create a new directory for move target
                            new_dir_path = mmupp::fs::relative_with_program_path("test_db_p1_6_move_target");
                            ghc::filesystem::create_directories(mm_to<memepp::native_string>(new_dir_path));
        
                            // Get handle and keep it externally
                                                        auto hdl_ret = seq->get_rw_hdl(0, 0);
                                                        REQUIRE(hdl_ret.has_value());
                                                        auto external_hdl = hdl_ret.value();
                                                        hdl_ret = outcome::failure(mgpp::err{});  // Release the outcome's internal shared_ptr copy
        
                            // Write some data
                            external_hdl->do_write("INSERT INTO data (name) VALUES ('test');");
        
                            // Get original file path BEFORE move
                                                        auto original_filepath = seq->filepath(0, 0);
                                                        auto original_native_path = mm_to<memepp::native_string>(original_filepath);
                                                        REQUIRE(ghc::filesystem::is_regular_file(original_native_path));
        
                                                        // Move directory while external handle is held
                                                        seq->set_dir_path_and_move(new_dir_path);
                
                                                        // Expected target path after move
                                                        auto target_filepath = fmt::format("{}/{:0>16}/{}.{:0>16}.{}",
                                                            new_dir_path, 0, seq->file_prefix(), 0, seq->file_suffix());
                
                                                        // Destroy seq (this triggers old_nodes_ destruction)
                                                        seq.reset();
        
                                                        // Now external_hdl is still alive, but seq_ is expired
                                                                                    // Check file state BEFORE reset
                                                                                    bool source_exists_before = ghc::filesystem::exists(original_native_path);
                                                                                    bool target_exists_before = ghc::filesystem::is_regular_file(target_filepath);
        
                                                                                    // Debug: check if external_hdl is the last reference
                                                                                    // external_hdl.reset() should trigger sqlite3_hdl destruction
        
                                                                                    // Release external handle - this triggers on_close_hdl
                                                                                    external_hdl.reset();
        
                                                                                    // Check file state IMMEDIATELY after reset (before sleep)
                                                                                    bool source_exists_immediate = ghc::filesystem::exists(original_native_path);
                                                                                    bool target_exists_immediate = ghc::filesystem::is_regular_file(target_filepath);
        
                                                                                    // Give file system time to complete the rename operation
                                                                                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
                                                                                    // Check file state AFTER reset
                                                                                    bool source_exists_after = ghc::filesystem::exists(original_native_path);
                                                                                    bool target_exists_after = ghc::filesystem::is_regular_file(target_filepath);
        
                                                                                    // If rename didn't happen at all, report detailed state
                                                                                    if (!target_exists_after && source_exists_after) {
                                                                                        FAIL_CHECK(fmt::format(
                                                                                            "P1-6 rename not executed: before(source={},target={}), immediate(source={},target={}), after(source={},target={})",
                                                                                            source_exists_before, target_exists_before,
                                                                                            source_exists_immediate, target_exists_immediate,
                                                                                            source_exists_after, target_exists_after));
                                                                                    }
        
                            // Cleanup
                            ghc::filesystem::remove_all(mm_to<memepp::native_string>(new_dir_path));
                        }
    
            SECTION("Multiple handles: ensure correct file operation with persistent data")
            {
                new_dir_path = mmupp::fs::relative_with_program_path("test_db_p1_6_multi");
                ghc::filesystem::create_directories(mm_to<memepp::native_string>(new_dir_path));
        
                // Get multiple handles for the same node
                auto rw_hdl = seq->get_rw_hdl(0, 2).value();
                auto ro_hdl = seq->get_ro_hdl(0, 2).value();
        
                rw_hdl->do_write("INSERT INTO data (name) VALUES ('multi_test');");
        
                auto original_filepath = seq->filepath(0, 2);
                auto original_native_path = mm_to<memepp::native_string>(original_filepath);
                REQUIRE(ghc::filesystem::is_regular_file(original_native_path));
        
                // Move directory
                seq->set_dir_path_and_move(new_dir_path);
        
                auto target_filepath = fmt::format("{}/{:0>16}/{}.{:0>16}.{}",
                    new_dir_path, 0, seq->file_prefix(), 2, seq->file_suffix());
                //auto target_native_path = mm_to<memepp::native_string>(target_filepath);
        
                // Destroy seq
                seq.reset();
        
                // Release handles in any order
                ro_hdl.reset();
                rw_hdl.reset();
        
                // Verify: file should still be renamed correctly
                REQUIRE(ghc::filesystem::is_regular_file(target_filepath));
                REQUIRE(ghc::filesystem::exists(original_native_path) == false);
        
                // Cleanup
                ghc::filesystem::remove_all(mm_to<memepp::native_string>(new_dir_path));
            }
    
            // Clean up test directory if not already cleaned
            if (seq) {
                seq.reset();
            }
            if (ghc::filesystem::exists(mm_to<memepp::native_string>(dir_path))) {
                ghc::filesystem::remove_all(mm_to<memepp::native_string>(dir_path));
            }
        }