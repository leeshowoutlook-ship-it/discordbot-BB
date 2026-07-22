#pragma once
#include "chips.h"
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string UC_STATS_FILE = "uc_stats.json";

static void load_uc_stats() {
    std::ifstream f(UC_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = uc_stats_data[uid];
            s.civ_games = v.value("civ_games", 0);
            s.civ_wins  = v.value("civ_wins",  0);
            s.spy_games = v.value("spy_games", 0);
            s.spy_wins  = v.value("spy_wins",  0);
        }
    } catch (...) {}
}

static void save_uc_stats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : uc_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"civ_games", s.civ_games}, {"civ_wins",  s.civ_wins},
                {"spy_games", s.spy_games}, {"spy_wins",  s.spy_wins}
            };
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(UC_STATS_FILE, j.dump(2));
}
