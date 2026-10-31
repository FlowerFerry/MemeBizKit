cmake_minimum_required(VERSION 3.20)

project(MemeBizKitTests)

enable_testing()

option(BUILD_TEST_MKHMAP "Build test_mkhmap" ON)
option(BUILD_TEST_MKHMAP_PERF "Build test_mkhmap_perf" ON)
option(BUILD_TEST_NESTED_HMAP "Build test_nested_hmap" ON)
option(BUILD_TEST_PASSIVE_TIMER "Build test_passive_timer" ON)
option(BUILD_TEST_UNIT_TEST "Build unit_test" ON)
option(BUILD_TEST_OBF_READDIR "Build obf_readdir" ON)
option(BUILD_TEST_SQLITE3_SEQUENCE "Build sqlite3_sequence" ON)

if(BUILD_TEST_MKHMAP)
    add_subdirectory(test_mkhmap)
endif()

if(BUILD_TEST_MKHMAP_PERF)
    add_subdirectory(test_mkhmap_perf)
endif()

if(BUILD_TEST_NESTED_HMAP)
    add_subdirectory(test_nested_hmap)
endif()

if(BUILD_TEST_PASSIVE_TIMER)
    add_subdirectory(test_passive_timer)
endif()

if(BUILD_TEST_UNIT_TEST)
    add_subdirectory(unit_test)
endif()

if(BUILD_TEST_OBF_READDIR)
    add_subdirectory(obf_readdir)
endif()

if(BUILD_TEST_SQLITE3_SEQUENCE)
    add_subdirectory(sqlite3_sequence)
endif()
