
cmake_minimum_required(VERSION 3.20)

project(obf_readdir)

set(MEMESTRING_SONAME_SUFFIX_ENABLE OFF)

include(FetchContent)

FetchContent_Declare(
	fmt
	GIT_REPOSITORY https://githubfast.com/fmtlib/fmt.git
	GIT_TAG 10.2.1
	SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/fmt-10.2.1
)
FetchContent_Populate(fmt)

FetchContent_Declare(
	MemeString
	GIT_REPOSITORY https://githubfast.com/FlowerFerry/MemeString.git
	GIT_TAG develop
	SOURCE_DIR ${CMAKE_SOURCE_DIR}/deps/MemeString
)
FetchContent_Populate(MemeString)

add_subdirectory(deps/MemeString)

add_executable(${PROJECT_NAME} main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE FMT_HEADER_ONLY)

target_include_directories(${PROJECT_NAME} PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR}/
	${CMAKE_CURRENT_SOURCE_DIR}/deps/fmt-10.2.1/include
	${CMAKE_CURRENT_SOURCE_DIR}/deps/MemeString/include
	${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
	${MEMESTRING_PROJECT_NAME}
)

add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:${MEMESTRING_PROJECT_NAME}>
	$<TARGET_FILE_DIR:${PROJECT_NAME}>
)
