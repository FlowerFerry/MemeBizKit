cmake_minimum_required(VERSION 3.20)
project(unit_test)

set(MMBK_GIT_HOST "github.com" CACHE STRING "Git host for fetching dependencies")

set(MMBK_CATCH2_REPO   "catchorg/Catch2"          CACHE STRING "")
set(MMBK_CATCH2_TAG    "v2.13.9"                   CACHE STRING "")

set(MMBK_MEMESTRING_REPO "FlowerFerry/MemeString"  CACHE STRING "")
set(MMBK_MEMESTRING_TAG  "develop"                 CACHE STRING "")

set(MMBK_OUTCOME_REPO      "ned14/outcome"             CACHE STRING "")
set(MMBK_OUTCOME_TAG       "v2.2.9"                    CACHE STRING "")

set(MMBK_QUICKCPPLIB_REPO  "ned14/quickcpplib"         CACHE STRING "")
set(MMBK_QUICKCPPLIB_TAG   "master"                    CACHE STRING "")

set(MMBK_FILESYSTEM_REPO "gulrak/filesystem"       CACHE STRING "")
set(MMBK_FILESYSTEM_TAG  "v1.5.14"                 CACHE STRING "")

set(MMBK_LIBUV_REPO    "libuv/libuv"               CACHE STRING "")
set(MMBK_LIBUV_TAG     "v1.48.0"                   CACHE STRING "")

set(MMBK_UVW_REPO      "skypjack/uvw"              CACHE STRING "")
set(MMBK_UVW_TAG       "v3.4.0_libuv_v1.48"        CACHE STRING "")

set(MMBK_MEMEUTILS_REPO  "FlowerFerry/MemeUtils"   CACHE STRING "")
set(MMBK_MEMEUTILS_TAG   "develop"                 CACHE STRING "")

set(MMBK_SQLITE3_URL     "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip" CACHE STRING "")
set(MMBK_SQLITE3_SHA256  "77823cb110929c2bcb0f5d48e4833b5c59a8a6e40cdea3936b99e199dbbe5784" CACHE STRING "")

set(FETCHCONTENT_TIMEOUT 120)
set(FETCHCONTENT_QUIET   FALSE)

include(FetchContent)

# ---------------------------------------------------------------------------
# Catch2 (header-only, single-include)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_CATCH2_REPO}
    GIT_TAG        ${MMBK_CATCH2_TAG}
    GIT_PROGRESS   TRUE
)
FetchContent_GetProperties(catch2)
if(NOT catch2_POPULATED)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/Catch2)
        set(catch2_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/Catch2)
        set(catch2_POPULATED TRUE)
    else()
        FetchContent_Populate(catch2)
    endif()
endif()

# ---------------------------------------------------------------------------
# MemeString (compiled library: provides mego/, megopp/, memepp/ headers)
# ---------------------------------------------------------------------------
set(MEMESTRING_SONAME_SUFFIX_ENABLE OFF CACHE BOOL "" FORCE)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/MemeString)
    set(memestring_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/MemeString)
    add_subdirectory(${memestring_SOURCE_DIR})
    set(memestring_POPULATED TRUE)
else()
    FetchContent_Declare(
        MemeString
        GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_MEMESTRING_REPO}
        GIT_TAG        ${MMBK_MEMESTRING_TAG}
        GIT_PROGRESS   TRUE
    )
    FetchContent_GetProperties(memestring)
    if(NOT memestring_POPULATED)
        FetchContent_MakeAvailable(MemeString)
    endif()
endif()

# ---------------------------------------------------------------------------
# quickcpplib (header-only, required by outcome)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/quickcpplib)
    set(quickcpplib_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/quickcpplib)
else()
    FetchContent_Declare(
        quickcpplib
        GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_QUICKCPPLIB_REPO}
        GIT_TAG        ${MMBK_QUICKCPPLIB_TAG}
        GIT_PROGRESS   TRUE
    )
    FetchContent_GetProperties(quickcpplib)
    if(NOT quickcpplib_POPULATED)
        FetchContent_Populate(quickcpplib)
    endif()
endif()

# ---------------------------------------------------------------------------
# outcome v2 (header-only)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/outcome)
    set(outcome_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/outcome)
else()
    FetchContent_Declare(
        outcome
        GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_OUTCOME_REPO}
        GIT_TAG        ${MMBK_OUTCOME_TAG}
        GIT_PROGRESS   TRUE
    )
    FetchContent_GetProperties(outcome)
    if(NOT outcome_POPULATED)
        FetchContent_Populate(outcome)
    endif()
endif()

# ---------------------------------------------------------------------------
# ghc/filesystem (header-only)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/filesystem)
    set(ghcfilesystem_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/filesystem)
else()
    FetchContent_Declare(
        ghcfilesystem
        GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_FILESYSTEM_REPO}
        GIT_TAG        ${MMBK_FILESYSTEM_TAG}
        GIT_PROGRESS   TRUE
    )
    FetchContent_GetProperties(ghcfilesystem)
    if(NOT ghcfilesystem_POPULATED)
        FetchContent_Populate(ghcfilesystem)
    endif()
endif()

# ---------------------------------------------------------------------------
# libuv (compiled C library, required by uvw)
# ---------------------------------------------------------------------------
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING     OFF CACHE BOOL "" FORCE)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/libuv)
    set(libuv_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/libuv)
    add_subdirectory(${libuv_SOURCE_DIR})
    set(libuv_POPULATED TRUE)
else()
    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_LIBUV_REPO}
        GIT_TAG        ${MMBK_LIBUV_TAG}
        GIT_PROGRESS   TRUE
    )
    FetchContent_GetProperties(libuv)
    if(NOT libuv_POPULATED)
        FetchContent_MakeAvailable(libuv)
    endif()
endif()

# ---------------------------------------------------------------------------
# uvw (C++ wrapper for libuv; compiled as static library with UVW_AS_LIB)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/uvw)
    set(uvw_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/uvw)
else()
    FetchContent_Declare(
        uvw
        GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_UVW_REPO}
        GIT_TAG        ${MMBK_UVW_TAG}
        GIT_PROGRESS   TRUE
    )
    FetchContent_GetProperties(uvw)
    if(NOT uvw_POPULATED)
        FetchContent_Populate(uvw)
    endif()
endif()

file(GLOB UVW_SOURCES "${uvw_SOURCE_DIR}/src/uvw/*.cpp")
add_library(uvw_impl STATIC ${UVW_SOURCES})
target_compile_features(uvw_impl PRIVATE cxx_std_17)
target_compile_definitions(uvw_impl PUBLIC UVW_AS_LIB)
target_include_directories(uvw_impl PUBLIC
    ${uvw_SOURCE_DIR}/src
    ${libuv_SOURCE_DIR}/include
)
target_link_libraries(uvw_impl PUBLIC uv_a)
if(MSVC)
    target_compile_options(uvw_impl PRIVATE /utf-8)
endif()

# ---------------------------------------------------------------------------
# MemeUtils (header-only part used: mmutilspp/locale/tag.h)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/MemeUtils)
    set(memeutils_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/MemeUtils)
else()
    FetchContent_Declare(
        MemeUtils
        GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_MEMEUTILS_REPO}
        GIT_TAG        ${MMBK_MEMEUTILS_TAG}
        GIT_PROGRESS   TRUE
    )
    FetchContent_GetProperties(memeutils)
    if(NOT memeutils_POPULATED)
        FetchContent_Populate(memeutils)
    endif()
endif()

# ---------------------------------------------------------------------------
# sqlite3 amalgamation (only for unit tests; catalog.h itself does not ship sqlite)
# ---------------------------------------------------------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/deps/sqlite3)
    set(sqlite3_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/sqlite3)
else()
    FetchContent_Declare(
        sqlite3
        URL        ${MMBK_SQLITE3_URL}
        URL_HASH   SHA256=${MMBK_SQLITE3_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_GetProperties(sqlite3)
    if(NOT sqlite3_POPULATED)
        FetchContent_Populate(sqlite3)
    endif()
endif()

add_library(sqlite3_amalg STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3_amalg PUBLIC ${sqlite3_SOURCE_DIR})
target_compile_definitions(sqlite3_amalg PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_DEFAULT_MEMSTATUS=0
)
if(MSVC)
    target_compile_options(sqlite3_amalg PRIVATE /W0)
endif()

# ---------------------------------------------------------------------------
# Main test executable
# ---------------------------------------------------------------------------
set(SRC_FILES
    main.cpp
    test_object_factory.cpp
    test_object_registrar.cpp
    test_request_handler.cpp
    test_text_file.cpp
    test_text_file_monitor.cpp
    test_stream_package_preproc.cpp
    test_i18n_catalog.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    FMT_HEADER_ONLY
    UVW_AS_LIB
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${catch2_SOURCE_DIR}/single_include
    ${memestring_SOURCE_DIR}/include
    ${memeutils_SOURCE_DIR}/include
    ${quickcpplib_SOURCE_DIR}/include
    ${outcome_SOURCE_DIR}/include
    ${ghcfilesystem_SOURCE_DIR}/include
    ${libuv_SOURCE_DIR}/include
    ${uvw_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${MEMESTRING_PROJECT_NAME}
    uvw_impl
    sqlite3_amalg
)

# Catch2 uses __android_log_write on Android; liblog must be linked explicitly
if(ANDROID)
    target_link_libraries(${PROJECT_NAME} PRIVATE log)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${MEMESTRING_PROJECT_NAME}>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
)
