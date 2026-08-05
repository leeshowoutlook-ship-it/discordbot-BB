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
    src/handlers_pet.cpp
    src/handlers_shop.cpp
    src/handlers_games.cpp
    src/handlers_adv.cpp
)
target_link_libraries(dcbot PRIVATE dpp::dpp nlohmann_json::nlohmann_json)

# 每個 .cpp 都會直接或間接 #include <dpp/dpp.h>、<nlohmann/json.hpp>、types.h，
# 13 個檔案各自重新解析一次這些大型標頭是編譯慢、吃記憶體的主因。
# 預編譯標頭讓這些內容只解析一次、其餘檔案共用結果。
target_precompile_headers(dcbot PRIVATE
    <dpp/dpp.h>
    <nlohmann/json.hpp>
    "${CMAKE_CURRENT_SOURCE_DIR}/src/types.h"
)

if(MSVC)
    target_compile_options(dcbot PRIVATE /utf-8 /Zm2000)
    target_link_options(dcbot PRIVATE /STACK:8388608)
endif()
