#pragma once
// ─── 妞妞（跟電腦莊家對戰）────────────────────────────────────────────────────
// 玩家和莊家各拿 5 張牌：任 3 張加起來是 10 的倍數就「有牛」，剩下 2 張的個位數就是牛幾（0＝牛牛）。
// 牌點：A=1、2～10 照面額、J/Q/K=10。特殊牌型：炸彈（4 張同點）> 五花牛（5 張都是 JQK）> 牛牛。（沒有五小牛）
// 同牌型比最大的那張（點數 K 最大，同點數比花色 ♠>♥>♣>♦）；炸彈比四張的點數。不會有平手。
// 倍數：無牛～牛7 ×1、牛8～牛9 ×2、牛牛 ×3、五花牛 ×5、炸彈 ×10；贏照自己的牌型倍數拿、輸照莊家的牌型倍數賠，
// 只要持有押注的籌碼就能玩，輸超過持有的部分直接扣成負債（跟射龍門撞柱一樣）。一局直接開完，不會有進行中的狀態（重開 bot 不怕卡住）。
#include "chips.h"
#include "shoot.h" // sh_card（撲克牌 emoji）、sh_rng
#include <array>

static const std::string NN_STATS_FILE = "niuniu_stats.json";
struct NnStats { int wins = 0, losses = 0; int64_t profit = 0; };
inline std::map<uint64_t, NnStats> nn_stats_data;

static void load_nn_stats() {
    std::ifstream f(NN_STATS_FILE);
    if (!f.is_open()) return;
    try {
        nlohmann::json j; f >> j;
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [k, v] : j.items()) {
            auto& s = nn_stats_data[std::stoull(k)];
            s.wins = v.value("wins", 0); s.losses = v.value("losses", 0); s.profit = v.value("profit", (int64_t)0);
        }
    } catch (...) {}
}
static void save_nn_stats() {
    nlohmann::json j;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        for (auto& [u, s] : nn_stats_data) j[std::to_string(u)] = {{"wins", s.wins}, {"losses", s.losses}, {"profit", s.profit}};
    }
    std::lock_guard<std::mutex> io_lk(io_mutex);
    atomic_write(NN_STATS_FILE, j.dump(2));
}

// 呼叫時不可持有 data_mutex
static std::string nn_stats_line(dpp::snowflake uid) {
    NnStats st;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = nn_stats_data.find((uint64_t)uid);
        if (it != nn_stats_data.end()) st = it->second;
    }
    int total = st.wins + st.losses;
    std::string wr = total > 0 ? std::to_string(st.wins * 100 / total) + "%" : "—";
    return std::to_string(st.wins) + " 勝 " + std::to_string(st.losses) + " 敗　勝率 **" + wr + "**　盈虧 **" + std::string(st.profit >= 0 ? "+" : "") + std::to_string(st.profit) + "** 碼";
}

// ─── 牌型判定 ─────────────────────────────────────────────────────────────────
// 牌的編碼跟射龍門一樣：rank = c/4+1（1～13），花色 c%4（0♠ 1♥ 2♦ 3♣）
enum { NN_NONE = 0, NN_NIUNIU = 10, NN_FLOWER = 11, NN_BOMB = 12 };
struct NnHand {
    int type = NN_NONE;          // 0 無牛／1～9 牛幾／10 牛牛／11 五花牛／12 炸彈
    int high = 0;                // 最大的那張牌（比大小用）
    int quad = 0;                // 炸彈的點數
    std::array<int, 5> order{};  // 顯示順序：湊成 10 的三張在前、剩下兩張在後
    bool has_trio = false;
};
static int nn_rank(int c) { return c / 4 + 1; }
static int nn_val(int c)  { return std::min(nn_rank(c), 10); }
static int nn_power(int c) { static const int SP[4] = {4, 3, 1, 2}; return nn_rank(c) * 10 + SP[c % 4]; } // ♠>♥>♣>♦
static NnHand nn_eval(const std::array<int, 5>& h) {
    NnHand r; r.order = h; r.high = h[0];
    int val_sum = 0; bool flower = true; std::map<int, int> cnt;
    for (int c : h) {
        if (nn_power(c) > nn_power(r.high)) r.high = c;
        val_sum += nn_val(c);
        if (nn_rank(c) < 11) flower = false;
        cnt[nn_rank(c)]++;
    }
    for (auto& [rk, n] : cnt) if (n == 4) { r.type = NN_BOMB; r.quad = rk; return r; }
    if (flower) { r.type = NN_FLOWER; return r; }
    int best = -1;
    for (int i = 0; i < 5; i++) for (int j = i + 1; j < 5; j++) for (int k = j + 1; k < 5; k++) {
        int t3 = nn_val(h[i]) + nn_val(h[j]) + nn_val(h[k]);
        if (t3 % 10 != 0) continue;
        int rest = (val_sum - t3) % 10;
        int t = rest == 0 ? NN_NIUNIU : rest;
        if (t > best) {
            best = t;
            int o = 0; r.order[o++] = h[i]; r.order[o++] = h[j]; r.order[o++] = h[k];
            for (int x = 0; x < 5; x++) if (x != i && x != j && x != k) r.order[o++] = h[x];
            r.has_trio = true;
        }
    }
    r.type = best < 0 ? NN_NONE : best;
    return r;
}
static const int NN_FEE_PCT = 3; // 玩家贏時抽水 %
static int nn_mult(int t) { return t == NN_BOMB ? 10 : t == NN_FLOWER ? 5 : t == NN_NIUNIU ? 3 : t >= 8 ? 2 : 1; }
static std::string nn_name(int t) {
    static const char* N[] = {"無牛", "牛一", "牛二", "牛三", "牛四", "牛五", "牛六", "牛七", "牛八", "牛九", "牛牛", "五花牛", "炸彈"};
    return (t >= 0 && t <= 12) ? N[t] : "?";
}
static bool nn_player_wins(const NnHand& p, const NnHand& d) {
    if (p.type != d.type) return p.type > d.type;
    if (p.type == NN_BOMB) return p.quad > d.quad;
    return nn_power(p.high) > nn_power(d.high);
}
static std::string nn_cards_text(const NnHand& h) {
    std::string s;
    for (int i = 0; i < 5; i++) {
        if (i == 3 && h.has_trio) s += "　｜　";
        else if (i > 0) s += " ";
        s += sh_card(h.order[i]);
    }
    return s;
}

// ─── 開一局（呼叫時不可持有 data_mutex）──────────────────────────────────────
// raw：玩家輸入的下注（數字或 ALL＝全部籌碼）。輸的時候照莊家牌型倍數扣，可能扣成負的。
// 失敗時 is_error=true，回傳只有文字的訊息（呼叫端用私人訊息回）。
static dpp::message nn_play(dpp::snowflake uid, dpp::snowflake ch, std::string raw,
                            const std::string& avatar, const std::string& display, bool& is_error) {
    is_error = true;
    auto fail = [](const std::string& t) { dpp::message m(t); return m; };
    std::string lo = raw; for (auto& c : lo) c = (char)std::tolower((unsigned char)c);
    bool all = (lo == "all" || lo == "allin");
    int64_t bet = all ? get_chips(uid) : (raw.empty() ? 0 : std::atoll(raw.c_str()));
    bool got_ticket = false;
    BetRoomCheck rc = resolve_bet_room(uid, ch, bet, got_ticket);
    if (rc != BetRoomCheck::OK)
        return fail(bet_room_check_msg(rc, "用法：`!妞妞 <籌碼>` 或 `/妞妞 籌碼:100`（ALL＝全押）\n-# 輸了照莊家牌型倍數賠（最多 10 倍），不夠的會扣成負債。"));
    if (bet <= 0) return fail("❌ 籌碼不足，沒有籌碼不能玩。");
    grant_lottery_ticket_if_needed(uid, got_ticket);

    std::array<int, 52> deck; for (int i = 0; i < 52; i++) deck[i] = i;
    std::shuffle(deck.begin(), deck.end(), sh_rng());
    std::array<int, 5> ph{}, dh{};
    for (int i = 0; i < 5; i++) { ph[i] = deck[i]; dh[i] = deck[5 + i]; }
    NnHand P = nn_eval(ph), D = nn_eval(dh);
    bool win = nn_player_wins(P, D);
    int mult = win ? nn_mult(P.type) : nn_mult(D.type);
    int64_t gross = bet * mult;
    int64_t fee = win ? gross * NN_FEE_PCT / 100 : 0; // 贏的時候抽水
    int64_t delta = win ? gross - fee : -gross;
    int64_t chips_after = 0;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        int64_t have = chip_data[uid].chips;
        if (have < bet)
            return fail("❌ 籌碼不足（你有 " + std::to_string(have) + " 碼）。");
        chip_data[uid].chips += delta;
        chips_after = chip_data[uid].chips;
        auto& st = nn_stats_data[(uint64_t)uid];
        if (win) st.wins++; else st.losses++;
        st.profit += delta;
    }
    save_chips();
    save_nn_stats();
    is_error = false;
    if (!win && chips_after <= 0) announce_bankrupt(uid, ch);

    dpp::embed e;
    e.set_title("🐂  妞妞").set_color(win ? 0x2ECC71 : 0xE74C3C);
    std::string d;
    d += "押注：**" + std::to_string(bet) + "** 碼\n\n";
    d += "**🏦 莊家**　" + nn_cards_text(D) + "\n→ **" + nn_name(D.type) + "**（×" + std::to_string(nn_mult(D.type)) + "）\n\n";
    d += "**🙋 你**　" + nn_cards_text(P) + "\n→ **" + nn_name(P.type) + "**（×" + std::to_string(nn_mult(P.type)) + "）\n\n";
    if (win) {
        d += "🎉 **你贏了！** " + nn_name(P.type) + " ×" + std::to_string(mult) + " → **+" + std::to_string(delta) + "** 碼";
        if (fee > 0) d += "\n-# 贏 " + std::to_string(gross) + " 碼，抽水 " + std::to_string(NN_FEE_PCT) + "%（" + std::to_string(fee) + " 碼）";
    }
    else     d += "💸 **莊家贏** " + nn_name(D.type) + " ×" + std::to_string(mult) + " → **" + std::to_string(delta) + "** 碼";
    d += "\n\n-# 倍數：無牛～牛7 ×1、牛8～9 ×2、牛牛 ×3、五花牛 ×5、炸彈 ×10（贏照你的牌、輸照莊家的牌，不夠扣會變負債；贏的時候抽水 3%）";
    e.set_description(d);
    e.add_field("💰 籌碼", std::to_string(chips_after) + " 碼", true);
    e.add_field("📊 累計", nn_stats_line(uid), true);
    if (!avatar.empty()) e.set_thumbnail(avatar);
    e.set_footer(dpp::embed_footer().set_text("👤 " + (display.empty() ? std::to_string((uint64_t)uid) : display)));

    dpp::message msg; msg.add_embed(e);
    std::string sid = std::to_string((uint64_t)uid);
    int64_t dbl = bet * 2;
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("再來一局")
        .set_id("nn_again_" + sid + "_" + std::to_string(bet)).set_style(dpp::cos_primary)
        .set_disabled(chips_after < bet));
    row.add_component(dpp::component().set_type(dpp::cot_button).set_label("雙倍下注（" + std::to_string(dbl) + "）")
        .set_id("nn_again_" + sid + "_" + std::to_string(dbl)).set_style(dpp::cos_success)
        .set_disabled(chips_after < dbl));
    msg.add_component(row);
    return msg;
}

// nn_again_<uid>_<bet>：回傳 true＝已處理
static bool nn_handle_button(const dpp::button_click_t& ev) {
    const std::string& cid = ev.custom_id;
    if (cid.rfind("nn_again_", 0) != 0) return false;
    std::string rest = cid.substr(9);
    size_t us = rest.find('_');
    if (us == std::string::npos) return true;
    dpp::snowflake owner(std::stoull(rest.substr(0, us)));
    const dpp::user& user = ev.command.get_issuing_user();
    if (user.id != owner) {
        ev.reply(dpp::ir_channel_message_with_source, dpp::message("❌ 這不是你的遊戲！").set_flags(dpp::m_ephemeral));
        return true;
    }
    std::string name = ev.command.member.get_nickname().empty() ? user.global_name : ev.command.member.get_nickname();
    if (name.empty()) name = user.username;
    bool err = false;
    dpp::message m = nn_play(user.id, ev.command.channel_id, rest.substr(us + 1), user.get_avatar_url(), name, err);
    if (err) m.set_flags(dpp::m_ephemeral);
    ev.reply(dpp::ir_channel_message_with_source, m);
    return true;
}
