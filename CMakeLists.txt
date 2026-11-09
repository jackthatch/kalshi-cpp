cmake_minimum_required(VERSION 3.20)
project(kalshi_perps CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(kalshi_core INTERFACE)
target_include_directories(kalshi_core INTERFACE include)

add_executable(test_types tests/test_types.cpp)
target_link_libraries(test_types PRIVATE kalshi_core)

add_executable(test_book tests/order_book.cpp)
target_link_libraries(test_book PRIVATE kalshi_core)

add_executable(bench_book benchmarks/bench_book.cpp)
target_link_libraries(bench_book PRIVATE kalshi_core)

enable_testing()
add_test(NAME types COMMAND test_types)
add_test(NAME book COMMAND test_book)