cmake_minimum_required(VERSION 3.20)

project(MemeBizKitTests)

enable_testing()

# Main unit_test only - other subprojects have their own CI jobs
add_subdirectory(unit_test)
