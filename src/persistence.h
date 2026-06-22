#pragma once
#include "helpers.h"
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string REG_FILE      = "registrations.json";
static const std::string TEAMS_FILE    = "proposed_teams.json";
static const std::string GIVEAWAY_FILE = "giveaways.json";

// ─── Registrations ────────────────────────────────────────────────────────────

static void save_registrations() {
    nlohmann::json j = nlohmann::json::array();
    std::lock_guard<std::mutex> lk(data_mutex);
    for (auto& r : registrations) {
        nlohmann::json slots_j = nlohmann::json::array();
        for (auto& [d, t] : r.slots)
            slots_j.push_back({d, t});
        j.push_back({
            {"id",         r.id},
            {"user_id",    (uint64_t)r.user_id},
            {"channel_id", (uint64_t)r.channel_id},
            {"username",   r.username},
            {"boss",       r.boss},
            {"slots",      slots_j},
            {"position",   r.position}
        });
    }
    uint64_t max_id = 0;
    for (auto& r : registrations) if (r.id > max_id) max_id = r.id;
    nlohmann::json root; root["records"] = j; root["max_id"] = max_id;
    atomic_write(REG_FILE, root.dump(2));
}

static void load_registrations() {
    std::ifstream f(REG_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json root; f >> root;
        std::lock_guard<std::mutex> lk(data_mutex);
        registrations.clear();
        uint64_t max_id = root.value("max_id", (uint64_t)0);
        for (auto& item : root["records"]) {
            Registration r;
            r.id         = item.value("id",         (uint64_t)0);
            r.user_id    = item.value("user_id",    (uint64_t)0);
            r.channel_id = item.value("channel_id", (uint64_t)0);
            r.username   = item.value("username",   std::string{});
            r.boss       = item.value("boss",        std::string{});
            r.position   = item.value("position",   std::string{});
            for (auto& s : item["slots"])
                r.slots.push_back({s[0].get<std::string>(), s[1].get<std::string>()});
            registrations.push_back(r);
        }
        if (max_id >= reg_counter.load()) reg_counter.store(max_id + 1);
    } catch (...) {}
}

// ─── Proposed teams ───────────────────────────────────────────────────────────

static nlohmann::json reg_to_json(const Registration& r) {
    nlohmann::json slots_j = nlohmann::json::array();
    for (auto& [d, t] : r.slots) slots_j.push_back({d, t});
    return {
        {"id",         r.id},
        {"user_id",    (uint64_t)r.user_id},
        {"channel_id", (uint64_t)r.channel_id},
        {"username",   r.username},
        {"boss",       r.boss},
        {"slots",      slots_j},
        {"position",   r.position}
    };
}

static Registration reg_from_json(const nlohmann::json& item) {
    Registration r;
    r.id         = item.value("id",         (uint64_t)0);
    r.user_id    = item.value("user_id",    (uint64_t)0);
    r.channel_id = item.value("channel_id", (uint64_t)0);
    r.username   = item.value("username",   std::string{});
    r.boss       = item.value("boss",        std::string{});
    r.position   = item.value("position",   std::string{});
    for (auto& s : item["slots"])
        r.slots.push_back({s[0].get<std::string>(), s[1].get<std::string>()});
    return r;
}

static void save_proposed_teams() {
    nlohmann::json j;
    std::lock_guard<std::mutex> lk(data_mutex);
    for (auto& [tid, pt] : proposed_teams) {
        nlohmann::json members_j = nlohmann::json::array();
        for (auto& m : pt.members) members_j.push_back(reg_to_json(m));
        j[std::to_string(tid)] = {
            {"id",        pt.id},
            {"boss",      pt.boss},
            {"day",       pt.day},
            {"time_slot", pt.time_slot},
            {"members",   members_j}
        };
    }
    atomic_write(TEAMS_FILE, j.dump(2));
}

static void load_proposed_teams() {
    std::ifstream f(TEAMS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        proposed_teams.clear(); proposed_slots.clear();
        uint64_t max_id = 0;
        for (auto& [k, v] : j.items()) {
            ProposedTeam pt;
            pt.id        = v.value("id",        (uint64_t)0);
            pt.boss      = v.value("boss",        std::string{});
            pt.day       = v.value("day",         std::string{});
            pt.time_slot = v.value("time_slot",   std::string{});
            for (auto& m : v["members"]) pt.members.push_back(reg_from_json(m));
            proposed_teams[pt.id] = pt;
            proposed_slots.insert({pt.boss, pt.day, pt.time_slot});
            if (pt.id > max_id) max_id = pt.id;
        }
        if (max_id >= team_counter.load()) team_counter.store(max_id + 1);
    } catch (...) {}
}

// ─── Giveaways ────────────────────────────────────────────────────────────────

static void save_giveaways() {
    nlohmann::json j;
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
