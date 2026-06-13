cmake_minimum_required(VERSION 3.20)
project(dcbot)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(dpp CONFIG REQUIRED)

add_executable(dcbot src/main.cpp)
target_link_libraries(dcbot PRIVATE dpp::dpp)

if(MSVC)
    target_compile_options(dcbot PRIVATE /utf-8)
endif()
