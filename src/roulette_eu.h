#pragma once
#include "chips.h"
#include <random>
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>

// ─── 迷你輪盤：共8格。3紅／3黑／1黃／1炸彈（不顯示號碼，只押顏色）。
//     下注確認當下就決定結果，動畫只是連續編輯訊息揭曉過程 ─────────────────────

static const std::string EUROULETTE_STATS_FILE = "euroulette_stats.json";
static const std::string EUROULETTE_GAMES_FILE = "euroulette_games.json";

// 固定順序：紅 黑 紅 黑 紅 黑 黃 炸彈（不顯示號碼，只看顏色）
static const std::array<std::string, 8> EU_WHEEL_COLORS = {
    "red","black","red","black","red","black","yellow","bomb"
};

static std::mt19937& eu_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}

// 回傳輪盤格子索引 0~7
static int eu_roll() {
    return std::uniform_int_distribution<int>(0, 7)(eu_rng());
}

static std::string eu_color_emoji(const std::string& color) {
    if (color == "red")    return "🔴";
    if (color == "black")  return "⚫";
    if (color == "yellow") return "🟡";
    return "💣"; // bomb
}

static std::string eu_color_label(const std::string& color) {
    if (color == "red")    return "紅";
    if (color == "black")  return "黑";
    if (color == "yellow") return "黃";
    return "炸彈";
}

// 格子的顯示文字，例如 "🔴 紅" 或 "💣 炸彈"
static std::string eu_slot_display(int slot_idx) {
    const std::string& c = EU_WHEEL_COLORS[slot_idx];
    return eu_color_emoji(c) + " " + eu_color_label(c);
}

static std::string eu_bet_type_label(const std::string& type, int /*number*/) {
    if (type == "red")    return "🔴 紅";
    if (type == "black")  return "⚫ 黑";
    if (type == "yellow") return "🟡 黃";
    return type;
}

static bool eu_check_win(const EuRouletteGame& g) {
    return g.bet_type == EU_WHEEL_COLORS[g.result]; // 炸彈不開放下注，撞到炸彈時任何押注類型都不會相符，自動輸
}

// 賠付總額（含本金）：紅/黑 1.5:1 → 2.5倍；黃 7:1 → 8倍
static int64_t eu_payout_total(int64_t bet, const std::string& bet_type) {
    if (bet_type == "yellow") return bet * 8;
    return bet * 5 / 2; // 紅/黑
}

// ─── Persistence ─────────────────────────────────────────────────────────────

static void load_euroulettestats() {
    std::ifstream f(EUROULETTE_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = euroulette_stats_data[uid];
            s.wins   = v.value("wins",   0);
            s.losses = v.value("losses", 0);
            s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}

static void save_euroulettestats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : euroulette_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"wins", s.wins}, {"losses", s.losses}, {"profit", s.profit}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(EUROULETTE_STATS_FILE, j.dump(2));
}

// 只在「下注類型還沒選」的等待階段持久化；一旦開始轉動（結果已定）就視為短暫動畫，不落地存檔
static void save_euroulette_games() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [gid, g] : euroulette_games) {
            if (g.result != -1) continue; // 轉動中不存檔
            j[std::to_string(gid)] = {
                {"id",           gid},
                {"uid",          (uint64_t)g.uid},
                {"ch",           (uint64_t)g.ch},
                {"bet",          g.bet},
                {"avatar_url",   g.avatar_url},
                {"display_name", g.display_name},
            };
        }
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(EUROULETTE_GAMES_FILE, j.dump(2));
}

static void load_euroulette_games() {
    std::ifstream f(EUROULETTE_GAMES_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        uint64_t max_id = 0;
        for (auto& [k, v] : j.items()) {
            EuRouletteGame g;
            g.id           = v.value("id",          (uint64_t)0);
            g.uid          = v.value("uid",          (uint64_t)0);
            g.ch           = v.value("ch",           (uint64_t)0);
            g.bet          = v.value("bet",          (int64_t)0);
            g.avatar_url   = v.value("avatar_url",  std::string{});
            g.display_name = v.value("display_name",std::string{});
            if (g.id == 0 || !g.uid) continue;
            euroulette_games[g.id] = g;
            user_euroulette[g.uid] = g.id;
            if (g.id > max_id) max_id = g.id;
        }
        if (max_id >= euroulette_counter.load()) euroulette_counter.store(max_id + 1);
    } catch (...) {}
}

// ─── Bet-type pick screen ──────────────────────────────────────────────────────

static dpp::message make_euroulette_pick_msg(uint64_t gid, int64_t bet,
                                             const std::string& avatar_url,
                                             const std::string& display_name,
                                             dpp::snowflake uid) {
    dpp::embed e;
    e.set_title("🎡  迷你輪盤").set_color(0x27AE60);
    e.set_description("**下注：" + std::to_string(bet) + " 碼**\n"
                      "8格輪盤：🔴紅×3／⚫黑×3／🟡黃×1／💣炸彈×1\n"
                      "請選擇押注顏色：");
    e.add_field("🔴紅／⚫黑", "各3/8機率，賠率 1.5:1（發2.5倍）", false);
    e.add_field("🟡黃", "1/8機率，賠率 7:1（發8倍）", false);
    e.add_field("💣 炸彈", "無法下注，撞到算大家都輸", false);
    if (!avatar_url.empty()) e.set_thumbnail(avatar_url);
    e.set_footer(dpp::embed_footer().set_text("👤 " + (display_name.empty() ? std::to_string((uint64_t)uid) : display_name)));

    dpp::message msg; msg.add_embed(e);
    std::string gid_s = std::to_string(gid);

    dpp::component row1; row1.set_type(dpp::cot_action_row);
    auto btn1 = [&](const std::string& lbl, const std::string& type, dpp::component_style sty) {
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(lbl)
         .set_id("er_pick_" + gid_s + "_" + type).set_style(sty);
        row1.add_component(b);
    };
    btn1("🔴 紅", "red",    dpp::cos_danger);
    btn1("⚫ 黑", "black",  dpp::cos_secondary);
    btn1("🟡 黃", "yellow", dpp::cos_primary);
    msg.add_component(row1);

    return msg;
}

// ─── Start a game ─────────────────────────────────────────────────────────────

static dpp::message start_euroulette(dpp::snowflake uid, dpp::snowflake ch, int64_t bet,
                                     const std::string& avatar_url = "",
                                     const std::string& display_name = "") {
    uint64_t gid;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = user_euroulette.find(uid);
        if (it != user_euroulette.end()) { euroulette_games.erase(it->second); user_euroulette.erase(it); }
        gid = euroulette_counter++;
        EuRouletteGame g;
        g.id = gid; g.uid = uid; g.ch = ch; g.bet = bet;
        g.avatar_url = avatar_url; g.display_name = display_name;
        euroulette_games[gid] = g;
        user_euroulette[uid] = gid;
    }
    add_chips(uid, -bet);
    save_euroulette_games();
    return make_euroulette_pick_msg(gid, bet, avatar_url, display_name, uid);
}

// ─── Spin animation frame ──────────────────────────────────────────────────────

static dpp::message make_euroulette_spin_frame_msg(const EuRouletteGame& g, int display_slot) {
    dpp::embed e;
    e.set_title("🎡  輪盤轉動中...").set_color(0xF1C40F);
    e.set_description("下注：**" + std::to_string(g.bet) + "** 碼　押注：**" +
                      eu_bet_type_label(g.bet_type, g.bet_number) + "**\n\n"
                      "目前指向：" + eu_slot_display(display_slot));
    if (!g.avatar_url.empty()) e.set_thumbnail(g.avatar_url);
    e.set_footer(dpp::embed_footer().set_text("👤 " + (g.display_name.empty() ? std::to_string((uint64_t)g.uid) : g.display_name)));
    dpp::message msg; msg.add_embed(e);
    return msg;
}

// ─── Final result message ──────────────────────────────────────────────────────

static dpp::message make_euroulette_result_msg(const EuRouletteGame& g, bool win, int64_t payout) {
    int64_t net = payout - g.bet;

    int er_w = 0, er_l = 0; int64_t er_profit_total = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& st = euroulette_stats_data[g.uid];
        er_w = st.wins; er_l = st.losses; er_profit_total = st.profit;
    }
    int er_total = er_w + er_l;
    char er_rate[16] = "0.0%";
    if (er_total > 0) snprintf(er_rate, sizeof(er_rate), "%.1f%%", er_w * 100.0 / er_total);

    dpp::embed e;
    e.set_title("🎡  輪盤結果").set_color(win ? 0x2ECC71 : 0xE74C3C);
    e.set_description("開獎：" + eu_slot_display(g.result) + "\n"
                      "你的押注：**" + eu_bet_type_label(g.bet_type, g.bet_number) + "**");
    e.add_field("📊  結果", win ? "🎉 中獎！" : "💀 未中獎", true);
    if (win) e.add_field("💰  獲得", "+**" + std::to_string(net) + "** 碼", true);
    else     e.add_field("💰  損失", "-**" + std::to_string(g.bet) + "** 碼", true);
    e.add_field("💼  持有", std::to_string(get_chips(g.uid)) + " 碼", false);
    if (er_total > 0) {
        std::string stats_str = "中獎/未中 " + std::to_string(er_w) + "/" + std::to_string(er_l)
            + "　勝率 " + std::string(er_rate)
            + "　盈虧 " + (er_profit_total >= 0 ? "+" : "") + std::to_string(er_profit_total) + " 碼";
        e.add_field("📊 轉盤統計", stats_str, false);
    }
    if (!g.avatar_url.empty()) e.set_thumbnail(g.avatar_url);
    e.set_footer(dpp::embed_footer().set_text("👤 " + (g.display_name.empty() ? std::to_string((uint64_t)g.uid) : g.display_name)));

    dpp::message msg; msg.add_embed(e);

    std::string uid_s = std::to_string((uint64_t)g.uid);
    std::string bet_s = std::to_string(g.bet);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🎡 再來一局（" + bet_s + "碼）")
        .set_id("er_again_" + uid_s + "_" + bet_s).set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 雙倍（" + std::to_string(g.bet * 2) + "碼）")
        .set_id("er_again_" + uid_s + "_" + std::to_string(g.bet * 2)).set_style(dpp::cos_danger)
        .set_disabled(get_chips(g.uid) < g.bet * 2));
    msg.add_component(row);

    return msg;
}

// ─── Spin loop ─────────────────────────────────────────────────────────────────

static const int EUROULETTE_SPIN_FRAMES = 6;

static void eu_start_spin(uint64_t gid) {
    dpp::timer tid = g_bot->start_timer([gid](dpp::timer t) {
        EuRouletteGame g; bool found = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = euroulette_games.find(gid);
            if (it == euroulette_games.end()) { g_bot->stop_timer(t); return; }
            g = it->second; found = true;
        }
        if (!found) { g_bot->stop_timer(t); return; }

        g.spin_frame++;
        if (g.spin_frame < EUROULETTE_SPIN_FRAMES) {
            { std::lock_guard<std::mutex> lk(data_mutex);
              auto it = euroulette_games.find(gid);
              if (it != euroulette_games.end()) it->second.spin_frame = g.spin_frame;
            }
            int display_slot;
            if (g.spin_frame == EUROULETTE_SPIN_FRAMES - 1) {
                display_slot = g.result;
            } else {
                do { display_slot = eu_roll(); } while (display_slot == g.last_display);
            }
            { std::lock_guard<std::mutex> lk2(data_mutex);
              auto it2 = euroulette_games.find(gid);
              if (it2 != euroulette_games.end()) it2->second.last_display = display_slot;
            }
            auto fmsg = make_euroulette_spin_frame_msg(g, display_slot);
            fmsg.id = g.msg_id; fmsg.channel_id = g.ch;
            g_bot->message_edit(fmsg);
            return;
        }

        // Final frame：揭曉真正結果並結算
        g_bot->stop_timer(t);
        bool win = eu_check_win(g);
        int64_t payout = win ? eu_payout_total(g.bet, g.bet_type) : 0;
        int64_t net = payout - g.bet;
        if (payout > 0) add_chips(g.uid, payout);
        bool bankrupt = !win && get_chips(g.uid) <= 0;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            euroulette_games.erase(gid);
            user_euroulette.erase(g.uid);
            auto& st = euroulette_stats_data[g.uid];
            if (win) st.wins++; else st.losses++;
            st.profit += net;
        }
        save_euroulette_games();
        save_euroulettestats();
        if (bankrupt) announce_bankrupt(g.uid, g.ch);

        auto fmsg = make_euroulette_result_msg(g, win, payout);
        fmsg.id = g.msg_id; fmsg.channel_id = g.ch;
        g_bot->message_edit(fmsg);
    }, 1);
    std::lock_guard<std::mutex> lk(data_mutex);
    auto it = euroulette_games.find(gid);
    if (it != euroulette_games.end()) it->second.timer_id = tid;
}

// 由 handler 呼叫：玩家選好下注類型（或送出號碼modal）後，決定結果並開始轉動動畫
static void eu_confirm_bet_and_spin(uint64_t gid, const std::string& bet_type, int bet_number,
                                    dpp::snowflake msg_id) {
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = euroulette_games.find(gid);
        if (it == euroulette_games.end()) return;
        it->second.bet_type   = bet_type;
        it->second.bet_number = bet_number;
        it->second.result     = eu_roll();
        it->second.spin_frame = 0;
        it->second.msg_id     = msg_id;
    }
    save_euroulette_games(); // 轉動中不落地，這次會把它從存檔移除
    eu_start_spin(gid);
}
