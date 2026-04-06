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
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/deps/Catch2
    GIT_PROGRESS   TRUE
)
FetchContent_GetProperties(catch2)
if(NOT catch2_POPULATED)
    FetchContent_Populate(catch2)
endif()

# ---------------------------------------------------------------------------
# MemeString (compiled library: provides mego/, megopp/, memepp/ headers)
# ---------------------------------------------------------------------------
set(MEMESTRING_SONAME_SUFFIX_ENABLE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    MemeString
    GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_MEMESTRING_REPO}
    GIT_TAG        ${MMBK_MEMESTRING_TAG}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/deps/MemeString
    GIT_PROGRESS   TRUE
)
FetchContent_GetProperties(memestring)
if(NOT memestring_POPULATED)
    FetchContent_MakeAvailable(MemeString)
endif()

# ---------------------------------------------------------------------------
# quickcpplib (header-only, required by outcome)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    quickcpplib
    GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_QUICKCPPLIB_REPO}
    GIT_TAG        ${MMBK_QUICKCPPLIB_TAG}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/deps/quickcpplib
    GIT_PROGRESS   TRUE
)
FetchContent_GetProperties(quickcpplib)
if(NOT quickcpplib_POPULATED)
    FetchContent_Populate(quickcpplib)
endif()

# ---------------------------------------------------------------------------
# outcome v2 (header-only)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    outcome
    GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_OUTCOME_REPO}
    GIT_TAG        ${MMBK_OUTCOME_TAG}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/deps/outcome
    GIT_PROGRESS   TRUE
)
FetchContent_GetProperties(outcome)
if(NOT outcome_POPULATED)
    FetchContent_Populate(outcome)
endif()

# ---------------------------------------------------------------------------
# ghc/filesystem (header-only)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    ghcfilesystem
    GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_FILESYSTEM_REPO}
    GIT_TAG        ${MMBK_FILESYSTEM_TAG}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/deps/filesystem
    GIT_PROGRESS   TRUE
)
FetchContent_GetProperties(ghcfilesystem)
if(NOT ghcfilesystem_POPULATED)
    FetchContent_Populate(ghcfilesystem)
endif()

# ---------------------------------------------------------------------------
# libuv (compiled C library, required by uvw)
# ---------------------------------------------------------------------------
set(LIBUV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING     OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    libuv
    GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_LIBUV_REPO}
    GIT_TAG        ${MMBK_LIBUV_TAG}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/deps/libuv
    GIT_PROGRESS   TRUE
)
FetchContent_GetProperties(libuv)
if(NOT libuv_POPULATED)
    FetchContent_MakeAvailable(libuv)
endif()

# ---------------------------------------------------------------------------
# uvw (C++ wrapper for libuv; compiled as static library with UVW_AS_LIB)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    uvw
    GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_UVW_REPO}
    GIT_TAG        ${MMBK_UVW_TAG}
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/deps/uvw
    GIT_PROGRESS   TRUE
)
FetchContent_GetProperties(uvw)
if(NOT uvw_POPULATED)
    FetchContent_Populate(uvw)
endif()

file(GLOB UVW_SOURCES "${CMAKE_SOURCE_DIR}/deps/uvw/src/uvw/*.cpp")
add_library(uvw_impl STATIC ${UVW_SOURCES})
target_compile_features(uvw_impl PRIVATE cxx_std_17)
target_compile_definitions(uvw_impl PUBLIC UVW_AS_LIB)
target_include_directories(uvw_impl PUBLIC
    ${CMAKE_SOURCE_DIR}/deps/uvw/src
    ${CMAKE_SOURCE_DIR}/deps/libuv/include
)
target_link_libraries(uvw_impl PUBLIC uv_a)
if(MSVC)
    target_compile_options(uvw_impl PRIVATE /utf-8)
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
    ${CMAKE_SOURCE_DIR}/deps/Catch2/single_include
    ${CMAKE_SOURCE_DIR}/deps/MemeString/include
    ${CMAKE_SOURCE_DIR}/deps/quickcpplib/include
    ${CMAKE_SOURCE_DIR}/deps/outcome/include
    ${CMAKE_SOURCE_DIR}/deps/filesystem/include
    ${CMAKE_SOURCE_DIR}/deps/libuv/include
    ${CMAKE_SOURCE_DIR}/deps/uvw/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${MEMESTRING_PROJECT_NAME}
    uvw_impl
)

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
