#pragma once
#include "helpers.h"
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string GIVEAWAY_FILE = "giveaways.json";

// ─── Giveaways ────────────────────────────────────────────────────────────────

static void save_giveaways() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [gid, gw] : giveaways) {
            nlohmann::json parts_j = nlohmann::json::array();
            for (auto& p : gw.participants) parts_j.push_back((uint64_t)p);
            j[std::to_string(gid)] = {
                {"id",               gw.id},
                {"channel_id",       (uint64_t)gw.channel_id},
                {"msg_id",           (uint64_t)gw.msg_id},
                {"host_id",          (uint64_t)gw.host_id},
                {"prize",            gw.prize},
                {"winner_count",     gw.winner_count},
                {"provider",         gw.provider},
                {"mention",          gw.mention},
                {"note",             gw.note},
                {"role_restriction", (uint64_t)gw.role_restriction},
                {"role_name",        gw.role_name},
                {"end_time",         (int64_t)gw.end_time},
                {"entry_cost",       gw.entry_cost},
                {"participants",     parts_j},
                {"ended",            gw.ended}
            };
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(GIVEAWAY_FILE, j.dump(2));
}

static void load_giveaways() {
    std::ifstream f(GIVEAWAY_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        giveaways.clear();
        uint64_t max_id = 0;
        for (auto& [k, v] : j.items()) {
            Giveaway gw;
            gw.id               = v.value("id",               (uint64_t)0);
            gw.channel_id       = v.value("channel_id",       (uint64_t)0);
            gw.msg_id           = v.value("msg_id",           (uint64_t)0);
            gw.host_id          = v.value("host_id",          (uint64_t)0);
            gw.prize            = v.value("prize",             std::string{});
            gw.winner_count     = v.value("winner_count",     1);
            gw.provider         = v.value("provider",          std::string{});
            gw.mention          = v.value("mention",           std::string{});
            gw.note             = v.value("note",              std::string{});
            gw.role_restriction = v.value("role_restriction", (uint64_t)0);
            gw.role_name        = v.value("role_name",         std::string{});
            gw.end_time         = (time_t)v.value("end_time", (int64_t)0);
            gw.entry_cost       = v.value("entry_cost",       (int64_t)0);
            gw.ended            = v.value("ended",             false);
            for (auto& p : v["participants"]) gw.participants.insert(dpp::snowflake(p.get<uint64_t>()));
            giveaways[gw.id] = gw;
            if (gw.id > max_id) max_id = gw.id;
        }
        if (max_id >= giveaway_counter.load()) giveaway_counter.store(max_id + 1);
    } catch (...) {}
}
