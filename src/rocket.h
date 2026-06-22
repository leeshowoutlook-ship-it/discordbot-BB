#pragma once
#include "chips.h"
#include <random>
#include <cmath>
#include <sstream>

// ─── Persistence ──────────────────────────────────────────────────────────────

static const std::string ROCKET_STATS_FILE = "rocketstats.json";

static void load_rocketstats() {
    std::ifstream f(ROCKET_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = rocket_stats_data[uid];
            s.wins   = v.value("wins",   0);
            s.losses = v.value("losses", 0);
            s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}

static void save_rocketstats() {
    nlohmann::json j;
    std::lock_guard<std::mutex> lk(data_mutex);
    for (auto& [uid, s] : rocket_stats_data)
        j[std::to_string((uint64_t)uid)] = {
            {"wins",   s.wins},
            {"losses", s.losses},
            {"profit", s.profit}};
    atomic_write(ROCKET_STATS_FILE, j.dump(2));
}

// ─── RNG ──────────────────────────────────────────────────────────────────────

static std::mt19937& rk_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

static bool rk_explodes() {
    return std::uniform_int_distribution<int>(1, 100)(rk_rng()) <= 30;
}

// ─── Emoji by altitude ────────────────────────────────────────────────────────

static std::string rk_emoji(int presses) {
    static const char* TABLE[] = {
        "🚀",
        "✨🚀",
        "🔥🚀",
        "🔥🔥🚀",
        "⚡🔥🚀",
        "⚡⚡🔥🚀",
        "💥⚡🔥🚀",
        "🌟💥⚡🔥🚀",
        "🌟🌟💥⚡🔥🚀",
        "🌠🌟💥⚡🔥🚀",
        "🎆🌠🌟💥⚡🔥🚀",
    };
    if (presses < 0)  presses = 0;
    if (presses > 10) presses = 10;
    return TABLE[presses];
}

// ─── Multiplier helpers ───────────────────────────────────────────────────────

static double rk_multiplier(int presses) { return std::pow(1.35, presses); }

static std::string rk_mult_str(int presses) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2fx", rk_multiplier(presses));
    return buf;
}

static int64_t rk_profit(int64_t bet, int presses) {
    return (int64_t)(bet * (rk_multiplier(presses) - 1.0));
}

// ─── Stats line ───────────────────────────────────────────────────────────────

static std::string rk_stats_line(dpp::snowflake uid) {
    RocketStats st;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = rocket_stats_data.find(uid);
        if (it != rocket_stats_data.end()) st = it->second;
    }
    int total = st.wins + st.losses;
    std::string wr = (total > 0) ? std::to_string(st.wins * 100 / total) + "%" : "—";
    std::string ps = (st.profit >= 0) ? "+" + std::to_string(st.profit) : std::to_string(st.profit);
    return "勝率 **" + wr + "**　盈虧 **" + ps + "** 碼";
}

// ─── Player identity ──────────────────────────────────────────────────────────

static void rk_set_user(dpp::embed& e, const RocketGame& rg) {
    if (!rg.avatar_url.empty()) e.set_thumbnail(rg.avatar_url);
    dpp::embed_footer footer;
    footer.text = "👤 " + (rg.display_name.empty() ? std::to_string((uint64_t)rg.uid) : rg.display_name);
    if (!rg.avatar_url.empty()) footer.icon_url = rg.avatar_url;
    e.set_footer(footer);
}

// ─── Replay row ───────────────────────────────────────────────────────────────

static void rk_add_replay_row(dpp::message& msg, const RocketGame& rg, int64_t cur_chips) {
    std::string sid = std::to_string((uint64_t)rg.uid);
    int64_t dbl = rg.bet * 2;
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("再來一局").set_id("rocket_again_" + sid + "_" + std::to_string(rg.bet))
        .set_style(dpp::cos_primary));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("雙倍下注（" + std::to_string(dbl) + "）")
        .set_id("rocket_again_" + sid + "_" + std::to_string(dbl))
        .set_style(dpp::cos_success)
        .set_disabled(cur_chips < dbl));
    msg.add_component(row);
}

// ─── Playing message ──────────────────────────────────────────────────────────

static dpp::message make_rocket_play_msg(const RocketGame& rg) {
    std::string emoji = rk_emoji(rg.presses);
    int64_t pot_profit = rk_profit(rg.bet, rg.presses);

    dpp::embed e;
    e.set_title(emoji + "  火箭升空").set_color(0xFF6B00);

    std::ostringstream desc;
    desc << "下注：**" << rg.bet << "** 碼\n";
    desc << "高度：**" << rg.presses << "** 層　倍率：**" << rk_mult_str(rg.presses) << "**\n\n";
    if (rg.presses == 0) {
        desc << "按「升空！」每次乘 **1.35x**，隨時可收手\n";
        desc << "每次有 **30%** 機率爆炸 💥\n";
        desc << "抵達第 10 層可獲得固定 **30x** 大獎 🌕\n";
    } else if (rg.presses == 9) {
        desc << "🌕 再升一次就登月！固定 **30x** 獎勵 **+" << rg.bet * 29 << "** 碼\n";
        desc << "💥 爆炸損失 **-" << rg.bet << "** 碼\n";
    } else {
        desc << "💰 收手可得 **+" << pot_profit << "** 碼\n";
        desc << "💥 爆炸損失 **-" << rg.bet << "** 碼\n";
    }
    e.set_description(desc.str());
    rk_set_user(e, rg);

    std::string sid = std::to_string((uint64_t)rg.uid);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚀 升空！").set_id("rocket_up_" + sid)
        .set_style(dpp::cos_danger)
        .set_disabled(rg.presses >= 10));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 收手").set_id("rocket_cash_" + sid)
        .set_style(dpp::cos_success)
        .set_disabled(rg.presses == 0));

    dpp::message msg; msg.add_embed(e); msg.add_component(row);
    return msg;
}

// ─── Explosion result (caller must NOT hold data_mutex) ───────────────────────

static dpp::message make_rocket_explode_msg(const RocketGame& rg) {
    int64_t missed = rk_profit(rg.bet, rg.presses);

    add_chips(rg.uid, -rg.bet);
    int64_t new_chips = get_chips(rg.uid);
    if (new_chips <= 0) announce_bankrupt(rg.uid, rg.channel_id);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = rocket_stats_data[rg.uid];
        st.losses++;
        st.profit -= rg.bet;
    }
    save_rocketstats();

    dpp::embed e;
    e.set_title("💥  爆炸了！").set_color(0xFF0000);

    std::ostringstream desc;
    desc << "高度：**" << rg.presses << "** 層　倍率：" << rk_mult_str(rg.presses) << "\n";
    if (rg.presses > 0)
        desc << "（差一點就能拿到 **+" << missed << "** 碼了！）\n";
    desc << "\n💸 損失 **-" << rg.bet << "** 碼\n";
    desc << "餘額：**" << new_chips << "** 碼\n";
    desc << rk_stats_line(rg.uid);
    e.set_description(desc.str());
    rk_set_user(e, rg);

    dpp::message msg; msg.add_embed(e);
    rk_add_replay_row(msg, rg, new_chips);
    return msg;
}

// ─── Moon landing (10 presses, fixed 30x) ────────────────────────────────────

static dpp::message make_rocket_moon_msg(const RocketGame& rg) {
    int64_t profit = rg.bet * 29;  // 30x total → +29x profit

    add_chips(rg.uid, profit);
    int64_t new_chips = get_chips(rg.uid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = rocket_stats_data[rg.uid];
        st.wins++;
        st.profit += profit;
    }
    save_rocketstats();

    dpp::embed e;
    e.set_title("🌕  恭喜成功登月！！！").set_color(0xF1C40F);

    std::ostringstream desc;
    desc << "高度：**10** 層　倍率：**30x**\n\n";
    desc << "🎉 獲得 **+" << profit << "** 碼\n";
    desc << "餘額：**" << new_chips << "** 碼\n";
    desc << rk_stats_line(rg.uid);
    e.set_description(desc.str());
    rk_set_user(e, rg);

    dpp::message msg; msg.add_embed(e);
    rk_add_replay_row(msg, rg, new_chips);
    return msg;
}

// ─── Cashout result (caller must NOT hold data_mutex) ─────────────────────────

static dpp::message make_rocket_cash_msg(const RocketGame& rg) {
    int64_t profit = rk_profit(rg.bet, rg.presses);

    add_chips(rg.uid, profit);
    int64_t new_chips = get_chips(rg.uid);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = rocket_stats_data[rg.uid];
        st.wins++;
        st.profit += profit;
    }
    save_rocketstats();

    dpp::embed e;
    e.set_title("💰  安全著陸！").set_color(0x2ECC71);

    std::ostringstream desc;
    desc << "高度：**" << rg.presses << "** 層　倍率：**" << rk_mult_str(rg.presses) << "**\n\n";
    desc << "🎉 獲得 **+" << profit << "** 碼\n";
    desc << "餘額：**" << new_chips << "** 碼\n";
    desc << rk_stats_line(rg.uid);
    e.set_description(desc.str());
    rk_set_user(e, rg);

    dpp::message msg; msg.add_embed(e);
    rk_add_replay_row(msg, rg, new_chips);
    return msg;
}

// ─── Start helper (called from main.cpp) ─────────────────────────────────────

static dpp::message handle_rocket_start(dpp::snowflake uid, dpp::snowflake ch, int64_t bet,
                                         const std::string& avatar_url = "",
                                         const std::string& display_name = "") {
    int64_t bal = get_chips(uid);
    if (bal < bet) {
        dpp::embed e;
        e.set_title("❌  籌碼不足").set_color(0xE74C3C);
        e.set_description("持有 **" + std::to_string(bal) + "** 碼，無法下注 **" + std::to_string(bet) + "** 碼");
        return dpp::message().add_embed(e);
    }

    RocketGame rg;
    rg.uid         = uid;
    rg.channel_id  = ch;
    rg.bet         = bet;
    rg.presses     = 0;
    rg.avatar_url  = avatar_url;
    rg.display_name = display_name;

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        rocket_games[uid] = rg;
    }
    return make_rocket_play_msg(rg);
}
