#pragma once
#include <dpp/dpp.h>
#include <string>
#include <algorithm>
#include <random>
#include <fstream>
#include "types.h"
#include "chips.h"
#include "persistence.h"

// ─── Core logic ───────────────────────────────────────────────────────────────

static std::string guess_gen_secret() {
    std::string d = "0123456789";
    std::mt19937 rng(std::random_device{}());
    std::shuffle(d.begin(), d.end(), rng);
    return d.substr(0, 4);
}

// Returns "xAyB"
static std::string guess_calc_ab(std::string secret, std::string guess) {
    int a = 0, b = 0;
    for (int i = 0; i < 4; i++) {
        if (guess[i] == secret[i]) { a++; guess[i] = secret[i] = 'X'; }
    }
    for (int i = 0; i < 4; i++) {
        if (guess[i] == 'X') continue;
        for (int j = 0; j < 4; j++) {
            if (secret[j] == 'X') continue;
            if (guess[i] == secret[j]) { b++; secret[j] = 'X'; break; }
        }
    }
    return std::to_string(a) + "A" + std::to_string(b) + "B";
}

static bool guess_valid(const std::string& s) {
    if (s.size() != 4) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    // no repeating
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            if (s[i] == s[j]) return false;
    return true;
}


// ─── Persist ──────────────────────────────────────────────────────────────────

static void load_guess_stats() {
    std::ifstream f("guess_stats.json");
    if (!f) return;
    nlohmann::json j; f >> j;
    std::lock_guard<std::mutex> lk(data_mutex);
    for (auto& [k, v] : j.items()) {
        uint64_t uid = std::stoull(k);
        guess_stats_data[uid].games              = v.value("games",              0);
        guess_stats_data[uid].wins               = v.value("wins",               0);
        guess_stats_data[uid].total_win_attempts = v.value("total_win_attempts", 0);
    }
}

static void save_guess_stats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [uid, st] : guess_stats_data)
            j[std::to_string(uid)] = {
                {"games",              st.games},
                {"wins",               st.wins},
                {"total_win_attempts", st.total_win_attempts}
            };
    }
    atomic_write("guess_stats.json", j.dump(2));
}

// ─── Stats line (for wallet – call outside data_mutex) ────────────────────────

static std::string guess_stats_line(dpp::snowflake uid) {
    int games = 0, wins = 0, twa = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = guess_stats_data.find((uint64_t)uid);
        if (it != guess_stats_data.end()) {
            games = it->second.games;
            wins  = it->second.wins;
            twa   = it->second.total_win_attempts;
        }
    }
    if (games == 0) return "尚無紀錄";
    char rate[16]; snprintf(rate, sizeof(rate), "%.1f%%", wins * 100.0 / games);
    std::string avg_str;
    if (wins > 0 && twa >= wins) {
        // twa < wins 代表有舊局未追蹤次數，跳過顯示
        char avg[16]; snprintf(avg, sizeof(avg), "%.1f", (double)twa / wins);
        avg_str = "　平均 **" + std::string(avg) + " 次**猜中";
    }
    return "解出/失敗 **" + std::to_string(wins) + "/" + std::to_string(games - wins) + "**"
         + "　解出率 **" + std::string(rate) + "**"
         + avg_str;
}

// ─── UI ───────────────────────────────────────────────────────────────────────

static dpp::message make_guess_msg(const GuessGame& g) {
    std::string uid_s = std::to_string((uint64_t)g.uid);
    bool full = g.input_buf.size() >= 4;

    dpp::embed e;
    e.set_title("🔢  猜數字").set_color(0x3498DB);
    e.set_description("🅰️ = 數字對且位置對　🅱️ = 數字對但位置錯");
    if (!g.avatar_url.empty()) e.set_thumbnail(g.avatar_url);

    // Current input display
    std::string inp_disp;
    for (int i = 0; i < 4; i++)
        inp_disp += (i < (int)g.input_buf.size())
            ? std::string(1, g.input_buf[i]) + " " : "\\_ ";
    e.add_field("⌨️ 當前輸入", "`" + inp_disp + "`", true);
    e.add_field("🎯 次數",
        std::to_string(g.attempts) + " / " + std::to_string(GuessGame::MAX_ATTEMPTS), true);

    // History
    std::string hist;
    for (auto& [gs, res] : g.history)
        hist += "`" + gs + "`　→　**" + res + "**\n";
    if (!hist.empty()) e.add_field("📋 猜測紀錄", hist, false);

    e.set_footer(dpp::embed_footer().set_text("👤 " + g.display_name));

    // ── Number pad ──
    auto mk_digit = [&](char d) -> dpp::component {
        bool used = g.input_buf.find(d) != std::string::npos;
        return dpp::component().set_type(dpp::cot_button)
            .set_label(std::string(1, d))
            .set_id("guess_digit_" + uid_s + "_" + d)
            .set_style(dpp::cos_secondary)
            .set_disabled(full || used);
    };

    dpp::component r1, r2, r3, r4, r5;
    r1.set_type(dpp::cot_action_row);
    r2.set_type(dpp::cot_action_row);
    r3.set_type(dpp::cot_action_row);
    r4.set_type(dpp::cot_action_row);
    r5.set_type(dpp::cot_action_row);

    r1.add_component(mk_digit('7')); r1.add_component(mk_digit('8')); r1.add_component(mk_digit('9'));
    r2.add_component(mk_digit('4')); r2.add_component(mk_digit('5')); r2.add_component(mk_digit('6'));
    r3.add_component(mk_digit('1')); r3.add_component(mk_digit('2')); r3.add_component(mk_digit('3'));

    r4.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⌨️").set_id("guess_kbd_" + uid_s)
        .set_style(dpp::cos_secondary));
    r4.add_component(mk_digit('0'));
    r4.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⌫").set_id("guess_back_" + uid_s)
        .set_style(dpp::cos_secondary)
        .set_disabled(g.input_buf.empty()));

    r5.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("✅ 確認").set_id("guess_confirm_" + uid_s)
        .set_style(dpp::cos_success)
        .set_disabled(!full));
    r5.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🏳️ 放棄").set_id("guess_quit_" + uid_s)
        .set_style(dpp::cos_danger));

    dpp::message m; m.add_embed(e);
    m.add_component(r1); m.add_component(r2); m.add_component(r3);
    m.add_component(r4); m.add_component(r5);
    return m;
}

static dpp::message make_guess_result_msg(const GuessGame& g, bool won) {
    std::string uid_s = std::to_string((uint64_t)g.uid);

    dpp::embed e;
    e.set_color(won ? 0x2ECC71 : 0xE74C3C);
    e.set_title(won ? "✅  猜中了！" : "❌  猜測失敗");
    if (!g.avatar_url.empty()) e.set_thumbnail(g.avatar_url);

    if (won)
        e.set_description("**第 " + std::to_string(g.attempts) + " 次**猜中！　答案是 `" + g.secret + "`");
    else
        e.set_description("10 次全用完，答案是 `" + g.secret + "`");

    std::string hist;
    for (auto& [gs, res] : g.history)
        hist += "`" + gs + "`　→　**" + res + "**\n";
    if (!hist.empty()) e.add_field("📋 猜測紀錄", hist, false);

    e.add_field("📊 個人統計", guess_stats_line(g.uid), false);
    e.set_footer(dpp::embed_footer().set_text("👤 " + g.display_name));

    dpp::message m; m.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔁 再來一局").set_id("guess_again_" + uid_s)
        .set_style(dpp::cos_success));
    m.add_component(row);
    return m;
}
