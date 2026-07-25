#pragma once
#include "chips.h"
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string RLSTATS_FILE = "rlstats.json";

static void save_roulettestats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : roulette_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"wins", s.wins}, {"losses", s.losses}, {"profit", s.profit}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(RLSTATS_FILE, j.dump(2));
}

static void load_roulettestats() {
    std::ifstream f(RLSTATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = roulette_stats_data[uid];
            s.wins   = v.value("wins",   0);
            s.losses = v.value("losses", 0);
            s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}
