#pragma once
#include "chips.h"
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string BJSTATS_FILE   = "bjstats.json";
static const std::string BJ_GAMES_FILE  = "bj_games.json";

static void load_bjstats() {
    std::ifstream f(BJSTATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = bj_stats_data[uid];
            s.wins   = v.value("wins",   0);
            s.losses = v.value("losses", 0);
            s.pushes = v.value("pushes", 0);
            s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}

static void save_bjstats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : bj_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"wins", s.wins}, {"losses", s.losses},
                {"pushes", s.pushes}, {"profit", s.profit}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(BJSTATS_FILE, j.dump(2));
}

// ─── BJ game persistence ──────────────────────────────────────────────────────

static nlohmann::json card_to_json(const BJCard& c) {
    return {{"r", c.rank}, {"s", c.suit}};
}
static BJCard card_from_json(const nlohmann::json& j) {
    return {j.value("r", 0), j.value("s", 0)};
}
static nlohmann::json hand_to_json(const BJHand& h) {
    nlohmann::json jc = nlohmann::json::array();
    for (auto& c : h.cards) jc.push_back(card_to_json(c));
    return {{"cards", jc}, {"doubled", h.doubled}, {"done", h.done}};
}
static BJHand hand_from_json(const nlohmann::json& j) {
    BJHand h;
    h.doubled = j.value("doubled", false);
    h.done    = j.value("done",    false);
    if (j.contains("cards")) for (auto& jc : j["cards"]) h.cards.push_back(card_from_json(jc));
    return h;
}

static void save_bj_games() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [gid, g] : bj_games) {
            if (g.game_over) continue; // don't persist finished games
            nlohmann::json deck = nlohmann::json::array();
            for (auto& c : g.deck) deck.push_back(card_to_json(c));
            nlohmann::json dealer = nlohmann::json::array();
            for (auto& c : g.dealer_cards) dealer.push_back(card_to_json(c));
            j[std::to_string(gid)] = {
                {"id",           gid},
                {"uid",          (uint64_t)g.user_id},
                {"ch",           (uint64_t)g.channel_id},
                {"msg_id",       (uint64_t)g.msg_id},
                {"bet",          g.bet},
                {"deck",         deck},
                {"main_hand",    hand_to_json(g.main_hand)},
                {"split_hand",   hand_to_json(g.split_hand)},
                {"has_split",    g.has_split},
                {"split_active", g.split_active},
                {"dealer_cards", dealer},
                {"avatar_url",   g.avatar_url},
                {"display_name", g.display_name},
            };
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(BJ_GAMES_FILE, j.dump(2));
}

static void load_bj_games() {
    std::ifstream f(BJ_GAMES_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        uint64_t max_id = 0;
        for (auto& [k, v] : j.items()) {
            BJGame g;
            g.id          = v.value("id",          (uint64_t)0);
            g.user_id     = v.value("uid",          (uint64_t)0);
            g.channel_id  = v.value("ch",           (uint64_t)0);
            g.msg_id      = v.value("msg_id",       (uint64_t)0);
            g.bet         = v.value("bet",          (int64_t)0);
            g.has_split   = v.value("has_split",    false);
            g.split_active= v.value("split_active", false);
            g.game_over   = false;
            g.avatar_url  = v.value("avatar_url",  std::string{});
            g.display_name= v.value("display_name",std::string{});
            if (v.contains("deck"))         for (auto& jc : v["deck"])         g.deck.push_back(card_from_json(jc));
            if (v.contains("dealer_cards")) for (auto& jc : v["dealer_cards"]) g.dealer_cards.push_back(card_from_json(jc));
            if (v.contains("main_hand"))    g.main_hand  = hand_from_json(v["main_hand"]);
            if (v.contains("split_hand"))   g.split_hand = hand_from_json(v["split_hand"]);
            if (g.id == 0 || !g.user_id) continue;
            bj_games[g.id]     = g;
            user_bj[g.user_id] = g.id;
            if (g.id > max_id) max_id = g.id;
        }
        if (max_id >= bj_counter.load()) bj_counter.store(max_id + 1);
    } catch (...) {}
}
