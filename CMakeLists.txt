cmake_minimum_required(VERSION 3.20)
project(dcbot)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(dpp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(dcbot
    src/main.cpp
    src/handlers_bj.cpp
    src/handlers_roulette.cpp
    src/handlers_wolf.cpp
    src/handlers_uc.cpp
    src/handlers_hunt.cpp
    src/handlers_raid.cpp
    src/handlers_dd.cpp
)
target_link_libraries(dcbot PRIVATE dpp::dpp nlohmann_json::nlohmann_json)

if(MSVC)
    target_compile_options(dcbot PRIVATE /utf-8 /Zm2000)
    target_link_options(dcbot PRIVATE /STACK:8388608)
endif()
