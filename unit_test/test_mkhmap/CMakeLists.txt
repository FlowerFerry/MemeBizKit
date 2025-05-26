
cmake_minimum_required(VERSION 3.20)
project(test_mkhmap)

set(MMBK_GIT_HOST "github.com" CACHE STRING "")

set(MMBK_CATCH2_REPO "catchorg/Catch2" CACHE STRING "")
set(MMBK_CATCH2_TAG "v2.13.9" CACHE STRING "")

set(FETCHCONTENT_TIMEOUT 120)
set(FETCHCONTENT_QUIET FALSE)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://${MMBK_GIT_HOST}/${MMBK_CATCH2_REPO}
    GIT_TAG ${MMBK_CATCH2_TAG}
)
FetchContent_GetProperties(catch2)
if(NOT catch2_POPULATED)
    FetchContent_Populate(catch2)
endif()

set(SRC_FILES
    main.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${catch2_SOURCE_DIR}/single_include
    ../../include
)
