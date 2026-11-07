cmake_minimum_required(VERSION 3.20)
project(kalshi_perps CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_types tests/test_types.cpp)
target_include_directories(test_types PRIVATE include)

add_executable(test_book tests/order_book.cpp)
target_include_directories(test_book PRIVATE include)

enable_testing()
add_test(NAME types COMMAND test_types)
add_test(NAME book COMMAND test_book)
