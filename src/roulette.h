#pragma once
#include "types.h"
#include <random>
#include <string>

// ─── RNG ──────────────────────────────────────────────────────────────────────

static std::mt19937& rl_rng() {
    static std::mt19937 g(std::random_device{}());
    return g;
}
static int rl_rand(int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(rl_rng());
}

// ─── Bet helpers ──────────────────────────────────────────────────────────────

static double rl_multiplier(const std::string& t) {
    if (t == "p1")             return 1.2;
    if (t == "p2")             return 1.15;
    if (t == "odd" || t == "even") return 1.95;
    return 5.5; // ch1~ch6
}

static std::string rl_bet_label(const std::string& t) {
    if (t == "p1")   return "玩家一（先手）贏 ×1.2";
    if (t == "p2")   return "玩家二（後手）贏 ×1.15";
    if (t == "odd")  return "奇數發（1·3·5）×1.95";
    if (t == "even") return "偶數發（2·4·6）×1.95";
    return "第" + t.substr(2) + "發 ×5.5";
}

// bullet_ch: 1-6, loser: 1=P1 2=P2
static bool rl_bet_wins(const std::string& t, int bullet_ch, int loser) {
    if (t == "p1")   return loser == 2;
    if (t == "p2")   return loser == 1;
    if (t == "odd")  return bullet_ch % 2 == 1;
    if (t == "even") return bullet_ch % 2 == 0;
    return std::stoi(t.substr(2)) == bullet_ch;
}

// ─── Turn checks ──────────────────────────────────────────────────────────────
// 本回合已射過至少一槍，才可以 PASS
static bool rl_can_pass(const RouletteRoom& r) {
    return r.shots_this_turn >= 1;
}

// 當前是第 6 發，且本回合已射過第 5 發 → 不能再射，只能 PASS
static bool rl_shoot_disabled(const RouletteRoom& r) {
    return r.current_chamber == 6
        && r.shot5_shooter == r.active_player
        && r.shots_this_turn > 0;
}

// ─── Chamber indicator ────────────────────────────────────────────────────────
static std::string rl_chambers_str(const RouletteRoom& r) {
    std::string s;
    for (int i = 1; i <= 6; i++) {
        if (i < r.current_chamber)
            s += "🟤 ";  // 已射（未命中）
        else if (i == r.current_chamber)
            s += "🔴 ";  // 當前
        else
            s += "⚫ ";  // 未射
    }
    return s;
}

// ─── Room embed ───────────────────────────────────────────────────────────────
static dpp::message make_roulette_room_msg(const RouletteRoom& r) {
    std::string ch_s = std::to_string((uint64_t)r.channel_id);
    bool both_in = (r.p1_uid != 0 && r.p2_uid != 0);

    dpp::embed e;
    e.set_title("🎲  俄羅斯輪盤").set_color(0xC0392B);

    std::string desc;
    desc += "**對賭籌碼：** " + std::to_string(r.stake) + " 碼\n\n";
    desc += "🔴 **玩家一（先手）：** " + r.p1_name + "\n";
    desc += "⚫ **玩家二（後手）：** "
          + (r.p2_uid ? r.p2_name : "*等待加入...*") + "\n";
    if (r.invited_uid) {
        desc += "\n🔒 邀請制 — 僅限 **" + (r.invited_name.empty()
            ? "<@" + std::to_string((uint64_t)r.invited_uid) + ">"
            : r.invited_name) + "** 加入";
    }
    desc += "\n\n> 旁觀者可在開始前下注，玩家本人不可下注";
    e.set_description(desc);

    if (!r.side_bets.empty()) {
        std::string bl;
        for (auto& b : r.side_bets)
            bl += "• **" + b.display_name + "**：" + rl_bet_label(b.bet_type)
                + "（" + std::to_string(b.amount) + " 碼）\n";
        e.add_field("📋 旁觀者下注", bl, false);
    }

    dpp::embed_footer f;
    f.text = "六發裝一顆｜開自己一槍或 PASS 給對手";
    e.set_footer(f);

    dpp::message msg; msg.add_embed(e);

    // Row 1: room management
    dpp::component row1; row1.set_type(dpp::cot_action_row);
    if (!both_in) {
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("🚪 加入房間").set_id("rl_join_" + ch_s)
            .set_style(dpp::cos_success));
    }
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💰 調整籌碼量").set_id("rl_stake_" + ch_s)
        .set_style(dpp::cos_secondary));
    if (both_in) {
        row1.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label("▶ 開始遊戲").set_id("rl_start_" + ch_s)
            .set_style(dpp::cos_primary));
    }
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗑 解散房間").set_id("rl_dissolve_" + ch_s)
        .set_style(dpp::cos_danger));
    row1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄 狀態刷新").set_id("rl_refresh_" + ch_s)
        .set_style(dpp::cos_secondary));
    msg.add_component(row1);

    // Row 2: P1/P2 win bets
    dpp::component row2; row2.set_type(dpp::cot_action_row);
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("玩家一贏 ×1.2").set_id("rl_bet_p1_" + ch_s)
        .set_style(dpp::cos_secondary));
    row2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("玩家二贏 ×1.15").set_id("rl_bet_p2_" + ch_s)
        .set_style(dpp::cos_secondary));
    msg.add_component(row2);

    // Row 3: Chamber select menu
    dpp::component row3; row3.set_type(dpp::cot_action_row);
    dpp::component ch_sel;
    ch_sel.set_type(dpp::cot_selectmenu)
          .set_id("rl_ch_sel_" + ch_s)
          .set_placeholder("🔫 子彈在第幾發？×5.5");
    for (int i = 1; i <= 6; i++) {
        std::string iv = std::to_string(i);
        ch_sel.add_select_option(dpp::select_option(
            "第" + iv + "發", iv, "子彈在第" + iv + "發（×5.5）"));
    }
    row3.add_component(ch_sel);
    msg.add_component(row3);

    // Row 4: Odd/Even bets
    dpp::component row4; row4.set_type(dpp::cot_action_row);
    row4.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("奇數發（1·3·5）×1.95").set_id("rl_bet_odd_" + ch_s)
        .set_style(dpp::cos_secondary));
    row4.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("偶數發（2·4·6）×1.95").set_id("rl_bet_even_" + ch_s)
        .set_style(dpp::cos_secondary));
    msg.add_component(row4);

    return msg;
}

// ─── In-game embed ────────────────────────────────────────────────────────────
// 規則：每回合必須先射一槍，miss 後可繼續射或 PASS（結束本輪，換對手從下一發開始）
static dpp::message make_roulette_game_msg(const RouletteRoom& r) {
    std::string ch_s = std::to_string((uint64_t)r.channel_id);

    std::string active_name   = (r.active_player == 1) ? r.p1_name   : r.p2_name;
    std::string active_avatar = (r.active_player == 1) ? r.p1_avatar : r.p2_avatar;

    dpp::embed e;
    e.set_title("🎲  俄羅斯輪盤 — 進行中").set_color(0xE74C3C);
    if (!active_avatar.empty()) e.set_thumbnail(active_avatar);

    std::string desc;
    desc += rl_chambers_str(r) + "\n\n";
    desc += "🔴 **玩家一（先手）** " + r.p1_name + "\n";
    desc += "⚫ **玩家二（後手）** " + r.p2_name + "\n\n";
    desc += "**第 " + std::to_string(r.current_chamber) + " 發**\n";

    bool shoot_off = rl_shoot_disabled(r);
    if (shoot_off) {
        desc += "⚠️ 已射過第 5 發，現在只能 PASS 給對手";
    } else if (r.shots_this_turn == 0) {
        desc += "🎯 輪到 **" + active_name + "**——必須先開一槍";
    } else {
        desc += "🎯 **" + active_name + "** 還活著！繼續開還是 PASS？";
    }
    e.set_description(desc);

    dpp::embed_footer f;
    f.text = "👤 " + active_name;
    e.set_footer(f);

    dpp::message msg; msg.add_embed(e);

    dpp::component row; row.set_type(dpp::cot_action_row);

    bool can_shoot = !shoot_off;
    bool can_pass  = rl_can_pass(r) || shoot_off;

    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔫 開自己一槍").set_id("rl_shoot_" + ch_s)
        .set_style(dpp::cos_danger)
        .set_disabled(!can_shoot));

    std::string pass_label = can_pass ? "🤚 PASS" : "🤚 PASS（先開一槍）";
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label(pass_label).set_id("rl_pass_" + ch_s)
        .set_style(dpp::cos_secondary)
        .set_disabled(!can_pass));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔄").set_id("rl_refresh_" + ch_s)
        .set_style(dpp::cos_secondary));

    msg.add_component(row);
    return msg;
}

// ─── Result embed ─────────────────────────────────────────────────────────────
static dpp::message make_roulette_result_msg(const RouletteRoom& r) {
    dpp::embed e;
    e.set_title("🎲  俄羅斯輪盤 — 結局").set_color(0x8E44AD);

    std::string loser_name   = (r.loser == 1) ? r.p1_name  : r.p2_name;
    std::string winner_name  = (r.loser == 1) ? r.p2_name  : r.p1_name;
    std::string loser_avatar = (r.loser == 1) ? r.p1_avatar : r.p2_avatar;
    if (!loser_avatar.empty()) e.set_thumbnail(loser_avatar);

    std::string desc;
    desc += rl_chambers_str(r) + "\n\n";
    desc += "🔫 子彈在**第 " + std::to_string(r.bullet_chamber) + " 發**"
          + "（" + (r.bullet_chamber % 2 == 1 ? "奇數" : "偶數") + "）\n\n";
    desc += "💀 **" + loser_name + "** 中彈！\n";
    desc += "🏆 **" + winner_name + "** 獲勝！";
    e.set_description(desc);

    std::string main_res;
    main_res += "✅ **" + winner_name + "** +" + std::to_string((int64_t)(r.stake * 0.95)) + " 碼（×1.95）\n";
    main_res += "❌ **" + loser_name  + "** -" + std::to_string(r.stake) + " 碼";
    e.add_field("🎯 主賽結算", main_res, false);

    if (!r.side_bets.empty()) {
        std::string sres;
        for (auto& b : r.side_bets) {
            bool won = rl_bet_wins(b.bet_type, r.bullet_chamber, r.loser);
            if (won) {
                int64_t net = (int64_t)(b.amount * rl_multiplier(b.bet_type)) - b.amount;
                sres += "✅ **" + b.display_name + "** +" + std::to_string(net) + " 碼（"
                      + rl_bet_label(b.bet_type) + "）\n";
            } else {
                sres += "❌ **" + b.display_name + "** -" + std::to_string(b.amount) + " 碼（"
                      + rl_bet_label(b.bet_type) + "）\n";
            }
        }
        e.add_field("📋 旁觀者下注結算", sres, false);
    }

    // 主賽雙方累計統計（呼叫方已持 data_mutex，直接讀）
    dpp::snowflake winner_uid = (r.loser == 1) ? r.p2_uid : r.p1_uid;
    dpp::snowflake loser_uid  = (r.loser == 1) ? r.p1_uid : r.p2_uid;
    auto stats_line = [](dpp::snowflake uid) -> std::string {
        auto it = roulette_stats_data.find(uid);
        if (it == roulette_stats_data.end()) return "首局";
        auto& s = it->second;
        int total = s.wins + s.losses;
        char rate[16] = "0.0%";
        if (total > 0) snprintf(rate, sizeof(rate), "%.1f%%", s.wins * 100.0 / total);
        return "勝/負 " + std::to_string(s.wins) + "/" + std::to_string(s.losses)
             + "　勝率 " + rate
             + "　盈虧 " + (s.profit >= 0 ? "+" : "") + std::to_string(s.profit) + " 碼";
    };
    e.add_field("📊 " + winner_name + " 統計", stats_line(winner_uid), false);
    e.add_field("📊 " + loser_name  + " 統計", stats_line(loser_uid),  false);

    dpp::message msg; msg.add_embed(e);
    return msg;
}

// ─── Bet modal helper ─────────────────────────────────────────────────────────
static void rl_open_bet_modal(const dpp::button_click_t& ev,
                              const std::string& bet_type, const std::string& ch_s)
{
    std::string uid_s = std::to_string((uint64_t)ev.command.get_issuing_user().id);
    dpp::interaction_modal_response modal(
        "rl_bet_m_" + bet_type + "_" + ch_s + "_" + uid_s,
        "下注：" + rl_bet_label(bet_type));
    modal.add_component(dpp::component()
        .set_type(dpp::cot_text)
        .set_id("amount")
        .set_label("下注金額（籌碼）")
        .set_min_length(1).set_max_length(15)
        .set_required(true)
        .set_text_style(dpp::text_short));
    ev.dialog(modal);
}
