#pragma once
#include "chips.h"
#include <random>
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>

static const std::string DICESTATS_FILE  = "dicestats.json";
static const std::string DICE_GAMES_FILE = "dice_games.json";

static void load_dicestats() {
    std::ifstream f(DICESTATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            dpp::snowflake uid(std::stoull(k));
            auto& s = dice_stats_data[uid];
            s.wins   = v.value("wins",   0);
            s.losses = v.value("losses", 0);
            s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}

static void save_dicestats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, s] : dice_stats_data)
            j[std::to_string((uint64_t)uid)] = {
                {"wins", s.wins}, {"losses", s.losses}, {"profit", s.profit}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(DICESTATS_FILE, j.dump(2));
}

static void save_dice_games() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [gid, g] : dice_games)
            j[std::to_string(gid)] = {
                {"id",           gid},
                {"uid",          (uint64_t)g.uid},
                {"ch",           (uint64_t)g.ch},
                {"bet",          g.bet},
                {"avatar_url",   g.avatar_url},
                {"display_name", g.display_name},
            };
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(DICE_GAMES_FILE, j.dump(2));
}

static void load_dice_games() {
    std::ifstream f(DICE_GAMES_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        uint64_t max_id = 0;
        for (auto& [k, v] : j.items()) {
            DiceGame g;
            g.id           = v.value("id",          (uint64_t)0);
            g.uid          = v.value("uid",          (uint64_t)0);
            g.ch           = v.value("ch",           (uint64_t)0);
            g.bet          = v.value("bet",          (int64_t)0);
            g.avatar_url   = v.value("avatar_url",  std::string{});
            g.display_name = v.value("display_name",std::string{});
            g.choice       = 0;
            if (g.id == 0 || !g.uid) continue;
            dice_games[g.id]  = g;
            user_dice[g.uid]  = g.id;
            if (g.id > max_id) max_id = g.id;
        }
        if (max_id >= dice_counter.load()) dice_counter.store(max_id + 1);
    } catch (...) {}
}

static std::string dice_str(int face) {
    std::string name = "D" + std::to_string(face);
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = emoji_name_map.find(name);
        if (it != emoji_name_map.end()) return it->second;
    }
    static const char* ALT[] = {"", "1️⃣","2️⃣","3️⃣","4️⃣","5️⃣","6️⃣"};
    return ALT[face];
}

static std::array<int,3> roll_three_dice() {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, 6);
    return {dist(rng), dist(rng), dist(rng)};
}

// ─── Choice screen ────────────────────────────────────────────────────────────

static dpp::message make_dice_pick_msg(uint64_t gid, int64_t bet,
                                       const std::string& avatar_url,
                                       const std::string& display_name,
                                       dpp::snowflake uid) {
    dpp::embed e;
    e.set_title("🎲  擲骰子").set_color(0x9B59B6);
    e.set_description("**下注：" + std::to_string(bet) + " 碼**\n請選擇押注選項：");
    e.add_field("① 小（3~10）",  "賠率 1:1",   true);
    e.add_field("② 大（11~18）", "賠率 1:1",   true);
    e.add_field("③ 豹子 3",      "賠率 1:220", true);
    e.add_field("④ 豹子 18",     "賠率 1:220", true);
    if (!avatar_url.empty()) e.set_thumbnail(avatar_url);
    e.add_field("​", "<@" + std::to_string((uint64_t)uid) + ">", false);

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    auto btn = [&](const std::string& lbl, int c, dpp::component_style sty) {
        dpp::component b;
        b.set_type(dpp::cot_button).set_label(lbl)
         .set_id("dc_" + std::to_string(gid) + "_" + std::to_string(c))
         .set_style(sty);
        row.add_component(b);
    };
    btn("① 小",   1, dpp::cos_primary);
    btn("② 大",   2, dpp::cos_primary);
    btn("③ 豹3",  3, dpp::cos_danger);
    btn("④ 豹18", 4, dpp::cos_danger);
    msg.add_component(row);
    return msg;
}

// ─── Start a dice game ────────────────────────────────────────────────────────

static dpp::message start_dice(dpp::snowflake uid, dpp::snowflake ch, int64_t bet,
                               const std::string& avatar_url = "",
                               const std::string& display_name = "") {
    uint64_t gid;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = user_dice.find(uid);
        if (it != user_dice.end()) { dice_games.erase(it->second); user_dice.erase(it); }
        gid = dice_counter++;
        DiceGame g;
        g.id = gid; g.uid = uid; g.ch = ch; g.bet = bet;
        g.avatar_url = avatar_url; g.display_name = display_name;
        dice_games[gid] = g;
        user_dice[uid] = gid;
    }
    add_chips(uid, -bet);
    save_dice_games();
    return make_dice_pick_msg(gid, bet, avatar_url, display_name, uid);
}

// ─── Resolve after player picks choice ───────────────────────────────────────

static dpp::message handle_dice_pick(uint64_t gid, int choice, dpp::snowflake uid) {
    DiceGame g;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = dice_games.find(gid);
        if (it == dice_games.end() || it->second.uid != uid) return {};
        g = it->second;
        dice_games.erase(it);
        user_dice.erase(uid);
    }

    auto [d1, d2, d3] = roll_three_dice();
    int total = d1 + d2 + d3;

    bool win = false;
    int64_t payout = 0;
    std::string result_text;

    if (choice == 1) {
        win = (total <= 10);
        payout = win ? g.bet * 2 : 0;
        result_text = win ? "🎉 小（3~10）你贏了！" : "💀 點數太大，莊家贏";
    } else if (choice == 2) {
        win = (total >= 11);
        payout = win ? g.bet * 2 : 0;
        result_text = win ? "🎉 大（11~18）你贏了！" : "💀 點數太小，莊家贏";
    } else if (choice == 3) {
        win = (total == 3);
        payout = win ? g.bet * 221 : 0;
        result_text = win ? "🌟 豹子 3！超大贏！" : "💀 未中豹子 3";
    } else {
        win = (total == 18);
        payout = win ? g.bet * 221 : 0;
        result_text = win ? "🌟 豹子 18！超大贏！" : "💀 未中豹子 18";
    }

    if (payout > 0) add_chips(uid, payout);
    if (!win && get_chips(uid) <= 0) announce_bankrupt(uid, g.ch);
    save_dice_games(); // game was erased above, this removes it from the save file
    int64_t net = (int64_t)payout - g.bet;

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& ds = dice_stats_data[uid];
        if (win) ds.wins++; else ds.losses++;
        ds.profit += net;
    }
    save_dicestats();

    // Read cumulative stats for display
    int d_wins = 0, d_losses = 0; int64_t d_profit_total = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto& ds = dice_stats_data[uid];
        d_wins = ds.wins; d_losses = ds.losses; d_profit_total = ds.profit;
    }
    int d_total_games = d_wins + d_losses;
    char d_rate[16] = "0.0%";
    if (d_total_games > 0)
        snprintf(d_rate, sizeof(d_rate), "%.1f%%", d_wins * 100.0 / d_total_games);

    dpp::embed e;
    e.set_title("🎲  骰子結果").set_color(win ? 0x2ECC71 : 0xE74C3C);
    std::string dice_display = dice_str(d1) + "  " + dice_str(d2) + "  " + dice_str(d3);
    e.add_field("🔢  合計",  std::to_string(total) + " 點", true);
    e.add_field("📊  結果",  result_text,              true);
    if (win)
        e.add_field("💰  獲得", "+**" + std::to_string(net) + "** 碼", true);
    else
        e.add_field("💰  損失", "-**" + std::to_string(g.bet) + "** 碼", true);
    e.add_field("💼  持有", std::to_string(get_chips(uid)) + " 碼", false);
    if (d_total_games > 0) {
        std::string stats_str = "勝/負 " + std::to_string(d_wins) + "/" + std::to_string(d_losses)
            + "　勝率 " + std::string(d_rate)
            + "　盈虧 " + (d_profit_total >= 0 ? "+" : "") + std::to_string(d_profit_total) + " 碼";
        e.add_field("📊 骰子統計", stats_str, false);
    }
    if (!g.avatar_url.empty()) e.set_thumbnail(g.avatar_url);
    e.add_field("​", "<@" + std::to_string((uint64_t)uid) + ">", false);

    dpp::message msg;
    msg.set_content(dice_display);
    msg.add_embed(e);

    // Post-game buttons
    dpp::component row; row.set_type(dpp::cot_action_row);
    std::string uid_s = std::to_string((uint64_t)uid);
    std::string bet_s = std::to_string(g.bet);

    dpp::component again_btn, double_btn;
    again_btn.set_type(dpp::cot_button)
             .set_label("🎲 再來一局（" + bet_s + "碼）")
             .set_id("dc_again_" + uid_s + "_" + bet_s)
             .set_style(dpp::cos_success);
    double_btn.set_type(dpp::cot_button)
              .set_label("💰 雙倍（" + std::to_string(g.bet * 2) + "碼）")
              .set_id("dc_again_" + uid_s + "_" + std::to_string(g.bet * 2))
              .set_style(dpp::cos_danger);
    row.add_component(again_btn);
    row.add_component(double_btn);
    msg.add_component(row);
    if (!win) {
        int gc = 0, hr = 0;
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (inventory_data.count(uid)) {
              auto& inv = inventory_data[uid];
              gc = inv.count("game_cancel") ? inv["game_cancel"] : 0;
              hr = inv.count("half_refund") ? inv["half_refund"] : 0;
          }
        }
        if (gc > 0 || hr > 0) {
            dpp::component gc_row; gc_row.set_type(dpp::cot_action_row);
            if (gc > 0) gc_row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("這局不算!!")
                .set_id("game_cancel_" + uid_s + "_di_" + bet_s)
                .set_style(dpp::cos_success));
            if (hr > 0) gc_row.add_component(dpp::component().set_type(dpp::cot_button)
                .set_label("對不起我錯了！！")
                .set_id("half_refund_" + uid_s + "_di_" + bet_s)
                .set_style(dpp::cos_primary));
            msg.add_component(gc_row);
        }
    }
    return msg;
}
