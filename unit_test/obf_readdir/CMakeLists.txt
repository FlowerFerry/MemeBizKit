
cmake_minimum_required(VERSION 3.20)

project(obf_readdir)

set(MEMESTRING_SONAME_SUFFIX_ENABLE OFF)

include(FetchContent)

FetchContent_Declare(
	fmt
	GIT_REPOSITORY https://github.com/fmtlib/fmt.git
	GIT_TAG 10.2.1
)
FetchContent_MakeAvailable(fmt)

# Use MemeString from parent build (already fetched by unit_test root)
if(NOT TARGET meme_string AND NOT TARGET libmeme_string)
    message(FATAL_ERROR "MemeString target not available. Build as part of unit_test root.")
endif()

add_executable(${PROJECT_NAME} main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE FMT_HEADER_ONLY)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_include_directories(${PROJECT_NAME} PRIVATE 
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/deps/MemeString/include
	${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

# Link against the MemeString library
if(TARGET libmeme_string)
    target_link_libraries(${PROJECT_NAME} PRIVATE libmeme_string)
elseif(TARGET meme_string)
    target_link_libraries(${PROJECT_NAME} PRIVATE meme_string)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN")
)
