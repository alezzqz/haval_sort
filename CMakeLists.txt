cmake_minimum_required(VERSION 3.20)

project(haval_sort LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(haval_sort haval_sort.cpp)
target_compile_options(haval_sort PRIVATE -Wall -Wextra -O2)

target_link_options(haval_sort PRIVATE -static -static-libgcc -static-libstdc++)
