cmake_minimum_required(VERSION 3.25)
project(flyio_challenges)

set(CMAKE_CXX_STANDARD 20)

include_directories("include")

add_executable(flyio_challenges main.cpp include/json.hpp node.h node.cpp)
