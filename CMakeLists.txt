cmake_minimum_required(VERSION 3.29)
project(pm)

set(CMAKE_CXX_STANDARD 26)

file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
        EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)


CPMAddPackage("gh:apolukhin/pfr_non_boost#master")

add_executable(pm main.cpp)

target_link_libraries(pm PRIVATE pfr::pfr)

#target_precompile_headers(pm PRIVATE )
