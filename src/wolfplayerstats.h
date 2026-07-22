#pragma once
#include "chips.h"
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string WOLF_PLAYER_STATS_FILE = "wolfplayerstats.json";

static void load_wolf_player_stats() {
    std::ifstream f(WOLF_PLAYER_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = wolf_player_stats_data[uid];
            s.good_games = v.value("good_games", 0);
            s.good_wins  = v.value("good_wins",  0);
            s.bad_games  = v.value("bad_games",  0);
            s.bad_wins   = v.value("bad_wins",   0);
        }
    } catch (...) {}
}

static void save_wolf_player_stats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : wolf_player_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"good_games", s.good_games}, {"good_wins", s.good_wins},
                {"bad_games",  s.bad_games},  {"bad_wins",  s.bad_wins}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(WOLF_PLAYER_STATS_FILE, j.dump(2));
}

// ─── Leaderboard message ─────────────────────────────────────────────────────

static dpp::message make_wolf_leaderboard_msg() {
    struct Entry {
        dpp::snowflake uid;
        int games, wins;
    };
    std::vector<Entry> good_board, bad_board;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : wolf_player_stats_data) {
            if (s.good_games >= 10) good_board.push_back({uid, s.good_games, s.good_wins});
            if (s.bad_games  >= 10) bad_board.push_back( {uid, s.bad_games,  s.bad_wins});
        }
    }
    auto by_rate = [](const Entry& a, const Entry& b) {
        double ra = (double)a.wins / a.games;
        double rb = (double)b.wins / b.games;
        return ra != rb ? ra > rb : a.games > b.games;
    };
    std::sort(good_board.begin(), good_board.end(), by_rate);
    std::sort(bad_board.begin(),  bad_board.end(),  by_rate);

    auto fmt = [](const std::vector<Entry>& board, int max) -> std::string {
        if (board.empty()) return "尚無符合資格的玩家（需滿 10 場）";
        std::string s;
        int rank = 1;
        for (auto& e : board) {
            if (rank > max) break;
            double rate = e.wins * 100.0 / e.games;
            char buf[64]; snprintf(buf, sizeof(buf), "%.1f%%", rate);
            std::string medal = (rank == 1) ? "🥇" : (rank == 2) ? "🥈" : (rank == 3) ? "🥉" : std::to_string(rank) + ".";
            s += medal + " <@" + std::to_string((uint64_t)e.uid) + "> — **" + buf
               + "** （" + std::to_string(e.wins) + "/" + std::to_string(e.games) + " 場）\n";
            rank++;
        }
        return s;
    };

    dpp::embed e;
    e.set_title("🐺  狼人殺榜單").set_color(0x8B0000);
    e.add_field("😇 好人勝率榜（場次 ≥ 10）", fmt(good_board, 10), false);
    e.add_field("🐺 壞人勝率榜（場次 ≥ 10）", fmt(bad_board,  10), false);
    return dpp::message().add_embed(e);
}

// ─── Per-player wolf stats (wallet page 3) ───────────────────────────────────

static dpp::message make_wallet_wolf_msg(dpp::snowflake uid) {
    WolfPlayerStats s;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = wolf_player_stats_data.find(uid);
        if (it != wolf_player_stats_data.end()) s = it->second;
    }

    auto fmt_rate = [](int w, int t) -> std::string {
        if (t == 0) return "—";
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f%%", w * 100.0 / t);
        return buf;
    };

    dpp::embed e;
    e.set_title("🐺  狼人殺紀錄").set_color(0x8B0000);
    e.set_description("<@" + std::to_string((uint64_t)uid) + ">");

    if (s.good_games > 0) {
        e.add_field("😇  好人陣營",
            "場次 **" + std::to_string(s.good_games) + "**　"
            "勝場 **" + std::to_string(s.good_wins) + "**　"
            "勝率 **" + fmt_rate(s.good_wins, s.good_games) + "**", false);
    } else {
        e.add_field("😇  好人陣營", "尚無紀錄", false);
    }
    if (s.bad_games > 0) {
        e.add_field("🐺  壞人陣營",
            "場次 **" + std::to_string(s.bad_games) + "**　"
            "勝場 **" + std::to_string(s.bad_wins) + "**　"
            "勝率 **" + fmt_rate(s.bad_wins, s.bad_games) + "**", false);
    } else {
        e.add_field("🐺  壞人陣營", "尚無紀錄", false);
    }

    std::string sid = std::to_string((uint64_t)uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("← 返回").set_id("wallet_games_" + sid).set_style(dpp::cos_secondary));
    dpp::message msg; msg.add_embed(e); msg.add_component(row);
    return msg;
}
