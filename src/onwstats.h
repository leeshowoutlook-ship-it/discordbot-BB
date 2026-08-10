#pragma once
#include "chips.h"
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string ONW_STATS_FILE = "onwstats.json";

static void load_onw_stats() {
    std::ifstream f(ONW_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = onw_stats_data[uid];
            s.wolf_games    = v.value("wolf_games",    0);
            s.wolf_wins     = v.value("wolf_wins",     0);
            s.village_games = v.value("village_games", 0);
            s.village_wins  = v.value("village_wins",  0);
            s.tanner_games  = v.value("tanner_games",  0);
            s.tanner_wins   = v.value("tanner_wins",   0);
        }
    } catch (...) {}
}

static void save_onw_stats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : onw_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"wolf_games",    s.wolf_games},
                {"wolf_wins",     s.wolf_wins},
                {"village_games", s.village_games},
                {"village_wins",  s.village_wins},
                {"tanner_games",  s.tanner_games},
                {"tanner_wins",   s.tanner_wins}
            };
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(ONW_STATS_FILE, j.dump(2));
}

static dpp::message make_wallet_onw_msg(dpp::snowflake uid) {
    ONWStats s;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = onw_stats_data.find(uid);
        if (it != onw_stats_data.end()) s = it->second;
    }

    auto fmt_rate = [](int w, int t) -> std::string {
        if (t == 0) return "—";
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f%%", w * 100.0 / t);
        return buf;
    };

    std::string content = "## 🌙 一夜狼人紀錄\n<@" + std::to_string((uint64_t)uid) + ">\n\n";
    content += "**🐺 狼人陣**\n";
    content += (s.wolf_games > 0)
        ? ("場次 **" + std::to_string(s.wolf_games) + "**　勝場 **" + std::to_string(s.wolf_wins) +
           "**　勝率 **" + fmt_rate(s.wolf_wins, s.wolf_games) + "**\n\n")
        : "尚無紀錄\n\n";
    content += "**🏘️ 村民陣**\n";
    content += (s.village_games > 0)
        ? ("場次 **" + std::to_string(s.village_games) + "**　勝場 **" + std::to_string(s.village_wins) +
           "**　勝率 **" + fmt_rate(s.village_wins, s.village_games) + "**\n\n")
        : "尚無紀錄\n\n";
    content += "**🩱 皮革匠**\n";
    content += (s.tanner_games > 0)
        ? ("場次 **" + std::to_string(s.tanner_games) + "**　獨贏 **" + std::to_string(s.tanner_wins) +
           "**　勝率 **" + fmt_rate(s.tanner_wins, s.tanner_games) + "**")
        : "尚無紀錄";

    dpp::component container;
    container.set_type(dpp::cot_container).set_accent(dpp::utility::rgb(0x2C, 0x3E, 0x50));
    container.add_component_v2(v2_section(content));

    dpp::message msg;
    msg.set_flags(dpp::m_using_components_v2);
    msg.add_component_v2(container);

    std::string sid = std::to_string((uint64_t)uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("← 返回").set_id("wallet_games_" + sid).set_style(dpp::cos_secondary));
    msg.add_component_v2(row);
    return msg;
}
